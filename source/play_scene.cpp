#include <cassert>
#include "play_scene.h"
#include "pxr_mathutil.h"

#include <iostream>


using namespace pxr;

PlayScene::PlayScene(pxr::Game* owner) :
  Scene(owner),
  _snake{},
  _snakeLength{0},
  _nextMoveDirection{Snake::WEST},
  _currentMoveDirection{Snake::WEST},
  _stepClock_s{0.f}
{}

bool PlayScene::onInit()
{
  _sk = static_cast<Snake*>(_owner);
  return true;
}

void PlayScene::onEnter()
{
  _stepClock_s = 0.f;
  _nextMoveDirection = Snake::WEST;
  _currentMoveDirection = Snake::WEST;
  initializeSnake();
}

void PlayScene::onUpdate(double now, float dt)
{
  handleInput();

  _stepClock_s += dt;
  if(_stepClock_s > Snake::stepPeriod_s){
    stepSnake();
    _stepClock_s = 0.f;
  }
}

void PlayScene::onDraw(double now, float dt, int screenid)
{
  gfx::clearScreenTransparent(screenid);
  drawSnake(screenid);
}

void PlayScene::onExit()
{
}

void PlayScene::initializeSnake()
{
  _snakeLength = Snake::babySnakeLength;
  for(int block {SNAKE_HEAD_BLOCK}; block < Snake::babySnakeLength; ++block){
    _snake[block]._col = Snake::snakeHeadSpawnCol + block;
    _snake[block]._row = Snake::snakeHeadSpawnRow;
  }
  updateSnakeBlockSpriteIDs();
}

void PlayScene::stepSnake()
{
  for(int block {_snakeLength - 1}; block > SNAKE_HEAD_BLOCK; --block){
    _snake[block]._row = _snake[block - 1]._row;
    _snake[block]._col = _snake[block - 1]._col;
  }
  switch(_currentMoveDirection){
    case Snake::NORTH:
      _snake[SNAKE_HEAD_BLOCK]._row++;
      if(_snake[SNAKE_HEAD_BLOCK]._row >= Snake::boardSize._y)
        _snake[SNAKE_HEAD_BLOCK]._row = 0;
      break;
    case Snake::SOUTH:
      _snake[SNAKE_HEAD_BLOCK]._row--;
      if(_snake[SNAKE_HEAD_BLOCK]._row < 0)
        _snake[SNAKE_HEAD_BLOCK]._row = Snake::boardSize._y - 1;
      break;
    case Snake::EAST:
      _snake[SNAKE_HEAD_BLOCK]._col++;
      if(_snake[SNAKE_HEAD_BLOCK]._col >= Snake::boardSize._x)
        _snake[SNAKE_HEAD_BLOCK]._col = 0;
      break;
    case Snake::WEST:
      _snake[SNAKE_HEAD_BLOCK]._col--;
      if(_snake[SNAKE_HEAD_BLOCK]._col < 0)
        _snake[SNAKE_HEAD_BLOCK]._col = Snake::boardSize._x;
      break;
    default:
      assert(0);
  }

  _currentMoveDirection = _nextMoveDirection;

  updateSnakeBlockSpriteIDs();
}

void PlayScene::handleInput()
{
  bool lkey{false}, rkey{false}, ukey{false}, dkey{false};
  if(pxr::input::isKeyPressed(Snake::moveLeftKey)) lkey = true;
  if(pxr::input::isKeyPressed(Snake::moveRightKey)) rkey = true;
  if(pxr::input::isKeyPressed(Snake::moveUpKey)) ukey = true;
  if(pxr::input::isKeyPressed(Snake::moveDownKey)) dkey = true;
  
  int sum {lkey + rkey + ukey + dkey};
  if(sum > 1) return;

  if(lkey && _currentMoveDirection != Snake::EAST) _nextMoveDirection = Snake::WEST;
  if(rkey && _currentMoveDirection != Snake::WEST) _nextMoveDirection = Snake::EAST;
  if(ukey && _currentMoveDirection != Snake::SOUTH) _nextMoveDirection = Snake::NORTH;
  if(dkey && _currentMoveDirection != Snake::NORTH) _nextMoveDirection = Snake::SOUTH;
}

Snake::Direction PlayScene::findNeighbourDirection(const SnakeBlock& self, const SnakeBlock& neighbour)
{
  int dr = self._row - neighbour._row; 
  int dc = self._col - neighbour._col; 
  assert(dr ^ dc);

  // handle wrapping; the only way the abs(dr) and abs(dc) can be greater than 1 is if we have 
  // wrapped around the map. We have to handle this to ensure the correct sprite is selected 
  // since sprite selection is based on relative position of the neighbours.
  //
  // So for example, if dr > 1, it must mean that the neighbour is more than 1 tile away from
  // the self tile, which can only happen if the neighbour has jumped to the other side of the
  // world. Further if dr > 1 then the neighbour is far south of the self tile, meaning the wrap
  // was from the top of the world to the bottom. Equivilent conditions can be reasoned for the
  // other wrap directions too.
  //
  // The solution is to pretend the neighbour is actually 1 tile away in the opposite direction
  // to what it actually is. So if the neighbour is far south of self, we pretend it is one
  // tile to the north.

  if(dr > 1) dr = -1;
  if(dr < -1) dr = 1;
  if(dc > 1) dc = -1;
  if(dc < -1) dc = 1;

  assert(dr == 1 || dr == -1 || dr == 0);
  assert(dc == 1 || dc == -1 || dc == 0);
  if(dr > 0) return Snake::SOUTH;
  if(dr < 0) return Snake::NORTH;
  if(dc > 0) return Snake::WEST;
  if(dc < 0) return Snake::EAST;
  assert(0);
}

void PlayScene::updateSnakeBlockSpriteIDs()
{
  Snake::Direction headDir, tailDir;

  tailDir = findNeighbourDirection(_snake[SNAKE_HEAD_BLOCK], _snake[SNAKE_HEAD_BLOCK + 1]);
  _snake[SNAKE_HEAD_BLOCK]._spriteid = Snake::snakeHeadBlockTree[tailDir];

  for(int block {SNAKE_HEAD_BLOCK + 1}; block < _snakeLength - 1; ++block){
    headDir = findNeighbourDirection(_snake[block], _snake[block - 1]);
    tailDir = findNeighbourDirection(_snake[block], _snake[block + 1]);
    _snake[block]._spriteid = Snake::snakeBodyBlockTree[headDir][tailDir];
  }

  headDir = findNeighbourDirection(_snake[_snakeLength - 1], _snake[_snakeLength - 2]);
  _snake[_snakeLength - 1]._spriteid = Snake::snakeTailBlockTree[headDir];
}

void PlayScene::drawSnake(int screenid)
{
  for(int block {SNAKE_HEAD_BLOCK}; block < _snakeLength; ++block){
    Vector2i position {
      _snake[block]._col * Snake::blockSize_rx,
      _snake[block]._row * Snake::blockSize_rx
    };

    //
    // smooth the motion between block positions by lerping between them.
    //
    float t = _stepClock_s / Snake::stepPeriod_s;
    float limit = static_cast<float>(Snake::blockSize_rx) - 1.f;
    switch(_currentMoveDirection){
      case Snake::NORTH:
        position._y += lerp(0.f, limit, t);
        break;
      case Snake::SOUTH:
        position._y -= lerp(0.f, limit, t);
        break;
      case Snake::EAST:
        position._x += lerp(0.f, limit, t);
        break;
      case Snake::WEST:
        position._x -= lerp(0.f, limit, t);
        break;
      default:
        assert(0);
    }

    gfx::drawSprite(
      position,
      _sk->getSpritesheetKey(Snake::SSID_SNAKES),
      _snake[block]._spriteid + (_sk->getSnakeHero() * Snake::SID_COUNT),
      screenid
    );
  }
}
