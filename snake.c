#include <gb/gb.h>
#include <stdint.h>

// ========== SIMPLE RNG (no srand/rand needed) ==========
static uint16_t rng_state = 0xACE1u;
uint8_t rand8(void)
{
	rng_state = rng_state * 1103515245u + 12345u;
	return (uint8_t)(rng_state >> 8);
}

// ========== GAME CONSTANTS ==========
#define GRID_W 20 // 160 / 8
#define GRID_H 18 // 144 / 8
#define PLAY_Y_MIN 1

#define MAX_SPRITES 40
#define FOOD_SPRITE_INDEX 39
#define MAX_SNAKE 360 // 39 segments max

#define FRAME_DELAY 6 // frames between movement steps

#define TILE_SNAKE 0
#define TILE_EMPTY 1
#define TILE_DIGIT_BASE 2
#define TILE_WALL TILE_SNAKE

typedef enum {
	STATE_PLAYING = 0,
	STATE_DYING,
	STATE_GAMEOVER
} GameState;

GameState game_state = STATE_PLAYING;

// death animation controls
#define DEATH_FREEZE_FRAMES 30   // ~0.5s at 60fps
#define BLINK_INTERVAL      5    // frames between blink toggles

uint8_t death_timer = 0;
uint8_t blink_on = 1;


// ========== TILE DATA ==========
const uint8_t tile_empty[16] = {
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00 };

const uint8_t tile_filled[16] = {
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00 };

const uint8_t food_tile[16] = {
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF,
	0xFF, 0xFF };

const uint8_t digit_tiles[10][16] = {
	// 0
	{0x3C,0x3C, 0x66,0x66, 0x6E,0x6E, 0x76,0x76, 0x66,0x66, 0x66,0x66, 0x3C,0x3C, 0x00,0x00},
	// 1
	{0x18,0x18, 0x38,0x38, 0x18,0x18, 0x18,0x18, 0x18,0x18, 0x18,0x18, 0x3C,0x3C, 0x00,0x00},
	// 2
	{0x3C,0x3C, 0x66,0x66, 0x06,0x06, 0x0C,0x0C, 0x18,0x18, 0x30,0x30, 0x7E,0x7E, 0x00,0x00},
	// 3
	{0x3C,0x3C, 0x66,0x66, 0x06,0x06, 0x1C,0x1C, 0x06,0x06, 0x66,0x66, 0x3C,0x3C, 0x00,0x00},
	// 4
	{0x0C,0x0C, 0x1C,0x1C, 0x3C,0x3C, 0x6C,0x6C, 0x7E,0x7E, 0x0C,0x0C, 0x0C,0x0C, 0x00,0x00},
	// 5
	{0x7E,0x7E, 0x60,0x60, 0x7C,0x7C, 0x06,0x06, 0x06,0x06, 0x66,0x66, 0x3C,0x3C, 0x00,0x00},
	// 6
	{0x1C,0x1C, 0x30,0x30, 0x60,0x60, 0x7C,0x7C, 0x66,0x66, 0x66,0x66, 0x3C,0x3C, 0x00,0x00},
	// 7
	{0x7E,0x7E, 0x66,0x66, 0x06,0x06, 0x0C,0x0C, 0x18,0x18, 0x18,0x18, 0x18,0x18, 0x00,0x00},
	// 8
	{0x3C,0x3C, 0x66,0x66, 0x66,0x66, 0x3C,0x3C, 0x66,0x66, 0x66,0x66, 0x3C,0x3C, 0x00,0x00},
	// 9
	{0x3C,0x3C, 0x66,0x66, 0x66,0x66, 0x3E,0x3E, 0x06,0x06, 0x0C,0x0C, 0x38,0x38, 0x00,0x00},
};

// ========== GAME STATE ==========
uint8_t snake_x[MAX_SNAKE];
uint8_t snake_y[MAX_SNAKE];
uint16_t snake_len;
uint8_t old_snake_pos_x;
uint8_t old_snake_pos_y;
uint16_t score = 0;
uint8_t last_tail_x, last_tail_y;
enum
{
	DIR_UP = 0,
	DIR_RIGHT,
	DIR_DOWN,
	DIR_LEFT
};
uint8_t dir = DIR_RIGHT;

uint8_t food_x, food_y;

// ========== UTILITY FUNCTIONS ==========
void clear_full_bg()
{
	for (uint8_t y = 0; y < GRID_H; y++)
		for (uint8_t x = 0; x < GRID_W; x++)
			set_bkg_tile_xy(x, y, TILE_EMPTY);
}
void clear_playspace()
{
	for (uint8_t y = PLAY_Y_MIN + 1; y < GRID_H - 1; y++)
		for (uint8_t x = 1; x < GRID_W - 1; x++)
			set_bkg_tile_xy(x, y, TILE_EMPTY);
}

void draw_box(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h)
{
	// simple rectangle border using TILE_SNAKE
	for (uint8_t x = x0; x < x0 + w; x++)
	{
		set_bkg_tile_xy(x, y0, TILE_SNAKE);
		set_bkg_tile_xy(x, y0 + h - 1, TILE_SNAKE);
	}
	for (uint8_t y = y0; y < y0 + h; y++)
	{
		set_bkg_tile_xy(x0, y, TILE_SNAKE);
		set_bkg_tile_xy(x0 + w - 1, y, TILE_SNAKE);
	}
}
void draw_border()
{
	uint8_t top = PLAY_Y_MIN;     // 1
	uint8_t bot = GRID_H - 1;     // 17

	// top + bottom edges
	for (uint8_t x = 0; x < GRID_W; x++)
	{
		set_bkg_tile_xy(x, top, TILE_SNAKE);
		set_bkg_tile_xy(x, bot, TILE_SNAKE);
	}

	// left + right edges
	for (uint8_t y = top; y <= bot; y++)
	{
		set_bkg_tile_xy(0, y, TILE_SNAKE);
		set_bkg_tile_xy(GRID_W - 1, y, TILE_SNAKE);
	}
}

void draw_score_ui()
{
	uint16_t s = score;
	if (s > 999) s = 999;

	uint8_t d0 = (uint8_t)(s % 10); s /= 10;
	uint8_t d1 = (uint8_t)(s % 10); s /= 10;
	uint8_t d2 = (uint8_t)(s % 10);

	set_bkg_tile_xy(2, 0, TILE_DIGIT_BASE + d2);
	set_bkg_tile_xy(3, 0, TILE_DIGIT_BASE + d1);
	set_bkg_tile_xy(4, 0, TILE_DIGIT_BASE + d0);
}

void reset_score()
{
	score = 0;
	// clear entire UI row (y=0)
	for (uint8_t x = 0; x < GRID_W; x++)
		set_bkg_tile_xy(x, 0, TILE_EMPTY);

	draw_score_ui();
}

void draw_game_over_splash()
{
	clear_playspace();

	// hide food
	move_sprite(FOOD_SPRITE_INDEX, 0, 0);

	//----------------------------------
	// centered box
	//----------------------------------
	uint8_t box_w = 12;
	uint8_t box_h = 7;

	uint8_t x0 = (GRID_W - box_w) / 2;
	uint8_t y0 = PLAY_Y_MIN + 4;

	draw_box(x0, y0, box_w, box_h);

	//----------------------------------
	// center score digits
	//----------------------------------
	uint16_t s = score;
	if (s > 999) s = 999;

	uint8_t d0 = (uint8_t)(s % 10); s /= 10;
	uint8_t d1 = (uint8_t)(s % 10); s /= 10;
	uint8_t d2 = (uint8_t)(s % 10);

	// digit row centered
	uint8_t digit_y = y0 + 3;
	uint8_t digit_x = x0 + (box_w / 2) - 2;

	set_bkg_tile_xy(digit_x + 0, digit_y, TILE_DIGIT_BASE + d2);
	set_bkg_tile_xy(digit_x + 1, digit_y, TILE_DIGIT_BASE + d1);
	set_bkg_tile_xy(digit_x + 2, digit_y, TILE_DIGIT_BASE + d0);

	//----------------------------------
	// press start bar
	//----------------------------------
	for (uint8_t x = x0 + 2; x < x0 + box_w - 2; x++)
		set_bkg_tile_xy(x, y0 + box_h - 2, TILE_SNAKE);
}
void start_death_sequence()
{
	game_state = STATE_DYING;
	death_timer = 0;
	blink_on = 1;

	// hide food immediately
	move_sprite(FOOD_SPRITE_INDEX, 0, 0);
}
void place_sprite_on_grid(uint8_t index, uint8_t gx, uint8_t gy)
{
	uint8_t px = gx * 8 + 8;
	uint8_t py = gy * 8 + 16;
	move_sprite(index, px, py);
}

uint8_t is_snake_body(uint8_t x, uint8_t y)
{
	for (uint8_t i = 0; i < snake_len; i++)
		if (snake_x[i] == x && snake_y[i] == y)
			return 1;
	return 0;
}

// place food somewhere random
void place_food_random()
{
	while (1)
	{
		food_x = 1 + (rand8() % (GRID_W - 2)); // 1..18
		food_y = (PLAY_Y_MIN + 1) + (rand8() % (GRID_H - (PLAY_Y_MIN + 2))); // 2..16

		if (!is_snake_body(food_x, food_y))
			break;
	}
	place_sprite_on_grid(FOOD_SPRITE_INDEX, food_x, food_y);
}

// setup snake in center
void init_snake()
{
	snake_len = 3;
	uint8_t cx = GRID_W / 2;
	uint8_t cy = (PLAY_Y_MIN + 1) + ((GRID_H - (PLAY_Y_MIN + 2)) / 2);

	// head at index 0, body extends left
	snake_x[0] = cx;
	snake_y[0] = cy;
	snake_x[1] = cx - 1;
	snake_y[1] = cy;
	snake_x[2] = cx - 2;
	snake_y[2] = cy;

	dir = DIR_RIGHT;
}

void draw_snake_full()
{
	// clear interior first
	clear_playspace();
	draw_border();

	for (uint16_t i = 0; i < snake_len; i++)
		set_bkg_tile_xy(snake_x[i], snake_y[i], TILE_SNAKE);
}

void get_next_head(int16_t* nx, int16_t* ny)
{
	int16_t hx = snake_x[0];
	int16_t hy = snake_y[0];

	if (dir == DIR_UP) hy--;
	else if (dir == DIR_DOWN) hy++;
	else if (dir == DIR_LEFT) hx--;
	else if (dir == DIR_RIGHT) hx++;

	*nx = hx;
	*ny = hy;
}

void draw_snake_head_tail(uint8_t grew)
{
	// draw new head
	set_bkg_tile_xy(snake_x[0], snake_y[0], TILE_SNAKE);

	// if we didn't grow, erase the old tail tile
	if (!grew)
	{
		// erase only interior (never border)
		if (last_tail_x > 0 && last_tail_x < (GRID_W - 1) &&
			last_tail_y > PLAY_Y_MIN && last_tail_y < (GRID_H - 1))
		{
			set_bkg_tile_xy(last_tail_x, last_tail_y, TILE_EMPTY);
		}
	}
}

// draw all segments
void draw_snake(uint8_t tile)
{
    for (uint16_t i = 0; i < snake_len; i++)
        set_bkg_tile_xy(snake_x[i], snake_y[i], tile);
}

uint8_t step_snake(uint8_t grow)
{
	// remember tail (last segment) so we can erase it (if not growing)
	last_tail_x = snake_x[snake_len - 1];
	last_tail_y = snake_y[snake_len - 1];

	// compute next head
	int16_t nx = snake_x[0];
	int16_t ny = snake_y[0];

	if (dir == DIR_UP) ny--;
	else if (dir == DIR_DOWN) ny++;
	else if (dir == DIR_LEFT) nx--;
	else if (dir == DIR_RIGHT) nx++;

	// wall collision: interior only
	if (nx <= 0 || nx >= (GRID_W - 1)) return 0;
	if (ny <= PLAY_Y_MIN || ny >= (GRID_H - 1)) return 0;

	// shift body
	// if growing, we keep the old tail by extending length first
	if (grow&& snake_len < MAX_SNAKE)
		snake_len++;

	for (int i = (int)snake_len - 1; i > 0; --i)
	{
		snake_x[i] = snake_x[i - 1];
		snake_y[i] = snake_y[i - 1];
	}

	// write new head
	snake_x[0] = (uint8_t)nx;
	snake_y[0] = (uint8_t)ny;

	return 1;
}
uint8_t head_hits_body()
{
	uint8_t hx = snake_x[0];
	uint8_t hy = snake_y[0];

	for (uint16_t i = 1; i < snake_len; i++)
		if (snake_x[i] == hx && snake_y[i] == hy)
			return 1;
	return 0;
}
void update_death_anim()
{
	// blink toggle
	if ((death_timer % BLINK_INTERVAL) == 0)
	{
		blink_on = !blink_on;

		if (blink_on)
		{
			// show snake
			draw_snake(TILE_SNAKE);
		}
		else
		{
			// hide snake 
			draw_snake(TILE_EMPTY);
		}
	}

	death_timer++;

	if (death_timer >= DEATH_FREEZE_FRAMES)
	{
		game_state = STATE_GAMEOVER;
		draw_game_over_splash(); // your centered-score splash version
	}
}
void begin_game()
{
	game_state = STATE_PLAYING;

	clear_full_bg();
	reset_score();
	draw_border();
	clear_playspace();

	init_snake();
	draw_snake(TILE_SNAKE);
	place_food_random();
}

// read input for direction
void handle_input()
{
	uint8_t j = joypad();

	if (game_state == STATE_GAMEOVER)
	{
		if (j & J_START)
			begin_game(); // full reset
		return;
	}

	if (game_state == STATE_DYING)
	{
		// ignore input while dying
		return;
	}

	// PLAYING: direction input (optional: add anti-reverse later)
	if ((j & J_UP) && dir != DIR_DOWN)  dir = DIR_UP;
	else if ((j & J_DOWN) && dir != DIR_UP)    dir = DIR_DOWN;
	else if ((j & J_LEFT) && dir != DIR_RIGHT) dir = DIR_LEFT;
	else if ((j & J_RIGHT) && dir != DIR_LEFT)  dir = DIR_RIGHT;
}
// ========== MAIN LOOP ==========
void main(void)
{
	rng_state = DIV_REG; // seed RNG with hardware divider
	game_state = STATE_PLAYING;

	set_sprite_data(0, 1, food_tile);

	set_bkg_data(TILE_SNAKE, 1, tile_filled);
	set_bkg_data(TILE_EMPTY, 1, tile_empty);
	set_bkg_data(TILE_DIGIT_BASE, 10, (const uint8_t*)digit_tiles);

	for (uint8_t i = 0; i < MAX_SPRITES; ++i)
	{
		set_sprite_tile(i, 0);
		move_sprite(i, 0, 0);
	}

	SHOW_SPRITES;
	SHOW_BKG;
	DISPLAY_ON;

	begin_game();

	uint16_t frame_counter = 0;

	while (1)
	{
		wait_vbl_done();
		frame_counter++;

		handle_input();

		if ((frame_counter % FRAME_DELAY) == 0)
		{
			if (game_state == STATE_PLAYING)
			{
				int16_t nx, ny;
				get_next_head(&nx, &ny);

				uint8_t will_grow = ((uint8_t)nx == food_x && (uint8_t)ny == food_y);

				if (!step_snake(will_grow))
				{
					start_death_sequence();
					continue;
				}

				if (head_hits_body())
				{
					start_death_sequence();
					continue;
				}

				if (will_grow)
				{
					score += 1;
					draw_score_ui();
					place_food_random();
				}

				draw_snake_head_tail(will_grow);
			}
			else if (game_state == STATE_DYING)
			{
				update_death_anim();
			}
		}
	}
}
