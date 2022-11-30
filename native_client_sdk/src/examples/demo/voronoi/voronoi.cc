// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <math.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/cpp/input_event.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_context_2d.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_interface.h"
#include "sdk_util/thread_pool.h"

using namespace sdk_util;  // For sdk_util::ThreadPool

// Global properties used to setup Voronoi demo.
namespace {
const int kMinRectSize = 4;
const int kStartRecurseSize = 32;  // must be power-of-two
const float kHugeZ = 1.0e38f;
const float kPI = M_PI;
const float kTwoPI = kPI * 2.0f;
const int kFramesToBenchmark = 100;
const unsigned int kRandomStartSeed = 0xC0DE533D;
const int kMaxPointCount = 1024;
const int kStartPointCount = 48;
const int kDefaultNumRegions = 256;

unsigned int g_rand_state = kRandomStartSeed;

// random number helper
inline unsigned char rand255() {
  return static_cast<unsigned char>(rand_r(&g_rand_state) & 255);
}

// random number helper
inline float frand() {
  return (static_cast<float>(rand_r(&g_rand_state)) /
      static_cast<float>(RAND_MAX));
}

// reset random seed
inline void rand_reset(unsigned int seed) {
  g_rand_state = seed;
}

#ifndef NDEBUG
// returns true if input is power of two.
// only used in assertions.
inline bool is_pow2(int x) {
  return (x & (x - 1)) == 0;
}
#endif

inline double getseconds() {
  const double usec_to_sec = 0.000001;
  timeval tv;
  if (0 == gettimeofday(&tv, NULL))
    return tv.tv_sec + tv.tv_usec * usec_to_sec;
  return 0.0;
}

// BGRA helper function, for constructing a pixel for a BGRA buffer.
inline uint32_t MakeBGRA(uint32_t b, uint32_t g, uint32_t r, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}
}  // namespace

// Vec2, simple 2D vector
struct Vec2 {
  float x, y;
  Vec2() {}
  Vec2(float px, float py) {
    x = px;
    y = py;
  }
  void Set(float px, float py) {
    x = px;
    y = py;
  }
};

// The main object that runs Voronoi simulation.
class Voronoi {
 public:
  Voronoi();
  virtual ~Voronoi();
  // Runs a tick of the simulations, update 2D output.
  void Update();
  // Handle event from user, or message from JS.
  void HandleEvent(PSEvent* ps_event);

 private:
  // Methods prefixed with 'w' are run on worker threads.
  uint32_t* wGetAddr(int x, int y);
  int wCell(float x, float y);
  inline void wFillSpan(uint32_t *pixels, uint32_t color, int width);
  void wRenderTile(int x, int y, int w, int h);
  void wProcessTile(int x, int y, int w, int h);
  void wSubdivide(int x, int y, int w, int h);
  void wMakeRect(int region, int *x, int *y, int *w, int *h);
  bool wTestRect(int *m, int x, int y, int w, int h);
  void wFillRect(int x, int y, int w, int h, uint32_t color);
  void wRenderRect(int x0, int y0, int x1, int y1);
  void wRenderRegion(int region);
  static void wRenderRegionEntry(int region, void *thiz);

  // These methods are only called by the main thread.
  void Reset();
  void UpdateSim();
  void RenderDot(float x, float y, uint32_t color1, uint32_t color2);
  void SuperimposePositions();
  void Render();
  void Draw();
  void StartBenchmark();
  void EndBenchmark();
  // Helper to post small update messages to JS.
  void PostUpdateMessage(const char* message_name, double value);

  PSContext2D_t* ps_context_;
  Vec2 positions_[kMaxPointCount];
  Vec2 screen_positions_[kMaxPointCount];
  Vec2 velocities_[kMaxPointCount];
  uint32_t colors_[kMaxPointCount];
  float ang_;
  const int num_regions_;
  int num_threads_;
  int point_count_;
  bool draw_points_;
  bool draw_interiors_;
  ThreadPool* workers_;
  int benchmark_frame_counter_;
  bool benchmarking_;
  double benchmark_start_time_;
  double benchmark_end_time_;
};


void Voronoi::Reset() {
  rand_reset(kRandomStartSeed);
  ang_ = 0.0f;
  for (int i = 0; i < kMaxPointCount; i++) {
    // random initial start position
    const float x = frand();
    const float y = frand();
    positions_[i].Set(x, y);
    // random directional velocity ( -1..1, -1..1 )
    const float speed = 0.0005f;
    const float u = (frand() * 2.0f - 1.0f) * speed;
    const float v = (frand() * 2.0f - 1.0f) * speed;
    velocities_[i].Set(u, v);
    // 'unique' color (well... unique enough for our purposes)
    colors_[i] = MakeBGRA(rand255(), rand255(), rand255(), 255);
  }
}

Voronoi::Voronoi() : num_regions_(kDefaultNumRegions), num_threads_(0),
    point_count_(kStartPointCount), draw_points_(true), draw_interiors_(true),
    benchmark_frame_counter_(0), benchmarking_(false)  {
  Reset();
  // By default, render from the dispatch thread.
  workers_ = new ThreadPool(num_threads_);
  PSEventSetFilter(PSE_ALL);
  ps_context_ = PSContext2DAllocate(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
}

Voronoi::~Voronoi() {
  delete workers_;
  PSContext2DFree(ps_context_);
}

inline uint32_t* Voronoi::wGetAddr(int x, int y) {
  return ps_context_->data + x + y * ps_context_->stride / sizeof(uint32_t);
}

// This is the core of the Voronoi calculation.  At a given point on the
// screen, iterate through all voronoi positions and render them as 3D cones.
// We're looking for the voronoi cell that generates the closest z value.
// (not really cones - since it is all relative, we avoid doing the
// expensive sqrt and test against z*z instead)
// If multithreading, this function is only called by the worker threads.
int Voronoi::wCell(float x, float y) {
  int closest_cell = 0;
  float zz = kHugeZ;
  Vec2* pos = screen_positions_;
  for (int i = 0; i < point_count_; ++i) {
    // measured 5.18 cycles per iteration on a core2
    float dx = x - pos[i].x;
    float dy = y - pos[i].y;
    float dd = (dx * dx + dy * dy);
    if (dd < zz) {
      zz = dd;
      closest_cell = i;
    }
  }
  return closest_cell;
}

// Given a region r, derive a non-overlapping rectangle for a thread to
// work on.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wMakeRect(int r, int* x, int* y, int* w, int* h) {
  const int parts = 16;
  assert(parts * parts == num_regions_);
  *w = ps_context_->width / parts;
  *h = ps_context_->height / parts;
  *x = *w * (r % parts);
  *y = *h * ((r / parts) % parts);
}

// Test 4 corners of a rectangle to see if they all belong to the same
// voronoi cell.  Each test is expensive so bail asap.  Returns true
// if all 4 corners match.
// If multithreading, this function is only called by the worker threads.
bool Voronoi::wTestRect(int* m, int x, int y, int w, int h) {
  // each test is expensive, so exit ASAP
  const int m0 = wCell(x, y);
  const int m1 = wCell(x + w - 1, y);
  if (m0 != m1) return false;
  const int m2 = wCell(x, y + h - 1);
  if (m0 != m2) return false;
  const int m3 = wCell(x + w - 1, y + h - 1);
  if (m0 != m3) return false;
  // all 4 corners belong to the same cell
  *m = m0;
  return true;
}

// Quickly fill a span of pixels with a solid color.  Assumes
// span width is divisible by 4.
// If multithreading, this function is only called by the worker threads.
inline void Voronoi::wFillSpan(uint32_t* pixels, uint32_t color, int width) {
  if (!draw_interiors_) {
    const uint32_t gray = MakeBGRA(128, 128, 128, 255);
    color = gray;
  }
  for (int i = 0; i < width; i += 4) {
    *pixels++ = color;
    *pixels++ = color;
    *pixels++ = color;
    *pixels++ = color;
  }
}

// Quickly fill a rectangle with a solid color.  Assumes
// the width w parameter is evenly divisible by 4.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wFillRect(int x, int y, int w, int h, uint32_t color) {
  const uint32_t stride_in_pixels = ps_context_->stride / sizeof(uint32_t);
  uint32_t* pixels = wGetAddr(x, y);
  for (int j = 0; j < h; j++) {
    wFillSpan(pixels, color, w);
    pixels += stride_in_pixels;
  }
}

// When recursive subdivision reaches a certain minimum without finding a
// rectangle that has four matching corners belonging to the same voronoi
// cell, this function will break the retangular 'tile' into smaller scanlines
//  and look for opportunities to quick fill at the scanline level.  If the
// scanline can't be quick filled, it will slow down even further and compute
// voronoi membership per pixel.
void Voronoi::wRenderTile(int x, int y, int w, int h) {
  // rip through a tile
  const uint32_t stride_in_pixels = ps_context_->stride / sizeof(uint32_t);
  uint32_t* pixels = wGetAddr(x, y);
  for (int j = 0; j < h; j++) {
    // get start and end cell values
    int ms = wCell(x + 0, y + j);
    int me = wCell(x + w - 1, y + j);
    // if the end points are the same, quick fill the span
    if (ms == me) {
      wFillSpan(pixels, colors_[ms], w);
    } else {
      // else compute each pixel in the span... this is the slow part!
      uint32_t* p = pixels;
      *p++ = colors_[ms];
      for (int i = 1; i < (w - 1); i++) {
        int m = wCell(x + i, y + j);
        *p++ = colors_[m];
      }
      *p++ = colors_[me];
    }
    pixels += stride_in_pixels;
  }
}

// Take a rectangular region and do one of -
//   If all four corners below to the same voronoi cell, stop recursion and
// quick fill the rectangle.
//   If the minimum rectangle size has been reached, break out of recursion
// and process the rectangle.  This small rectangle is called a tile.
//   Otherwise, keep recursively subdividing the rectangle into 4 equally
// sized smaller rectangles.
// Note: at the moment, these will always be squares, not rectangles.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wSubdivide(int x, int y, int w, int h) {
  int m;
  // if all 4 corners are equal, quick fill interior
  if (wTestRect(&m, x, y, w, h)) {
    wFillRect(x, y, w, h, colors_[m]);
  } else {
    // did we reach the minimum rectangle size?
    if ((w <= kMinRectSize) || (h <= kMinRectSize)) {
      wRenderTile(x, y, w, h);
    } else {
      // else recurse into smaller rectangles
      const int half_w = w / 2;
      const int half_h = h / 2;
      wSubdivide(x, y, half_w, half_h);
      wSubdivide(x + half_w, y, half_w, half_h);
      wSubdivide(x, y + half_h, half_w, half_h);
      wSubdivide(x + half_w, y + half_h, half_w, half_h);
    }
  }
}

// This function cuts up the rectangle into power of 2 sized squares.  It
// assumes the input rectangle w & h are evenly divisible by
// kStartRecurseSize.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wRenderRect(int x, int y, int w, int h) {
  for (int iy = y; iy < (y + h); iy += kStartRecurseSize) {
    for (int ix = x; ix < (x + w); ix += kStartRecurseSize) {
      wSubdivide(ix, iy, kStartRecurseSize, kStartRecurseSize);
    }
  }
}

// If multithreading, this function is only called by the worker threads.
void Voronoi::wRenderRegion(int region) {
  // convert region # into x0, y0, x1, y1 rectangle
  int x, y, w, h;
  wMakeRect(region, &x, &y, &w, &h);
  // render this rectangle
  wRenderRect(x, y, w, h);
}

// Entry point for worker thread.  Can't pass a member function around, so we
// have to do this little round-about.
void Voronoi::wRenderRegionEntry(int region, void* thiz) {
  static_cast<Voronoi*>(thiz)->wRenderRegion(region);
}

// Function Voronoi::UpdateSim()
// Run a simple sim to move the voronoi positions.  This update loop
// is run once per frame.  Called from the main thread only, and only
// when the worker threads are idle.
void Voronoi::UpdateSim() {
  ang_ += 0.002f;
  if (ang_ > kTwoPI) {
    ang_ = ang_ - kTwoPI;
  }
  float z = cosf(ang_) * 3.0f;
  // push the points around on the screen for animation
  for (int j = 0; j < kMaxPointCount; j++) {
    positions_[j].x += (velocities_[j].x) * z;
    positions_[j].y += (velocities_[j].y) * z;
    screen_positions_[j].x = positions_[j].x * ps_context_->width;
    screen_positions_[j].y = positions_[j].y * ps_context_->height;
  }
}

// Renders a small diamond shaped dot at x, y clipped against the window
void Voronoi::RenderDot(float x, float y, uint32_t color1, uint32_t color2) {
  const int ix = static_cast<int>(x);
  const int iy = static_cast<int>(y);
  const uint32_t stride_in_pixels = ps_context_->stride / sizeof(uint32_t);
  // clip it against window
  if (ix < 1) return;
  if (ix >= (ps_context_->width - 1)) return;
  if (iy < 1) return;
  if (iy >= (ps_context_->height - 1)) return;
  uint32_t* pixel = wGetAddr(ix, iy);
  // render dot as a small diamond
  *pixel = color1;
  *(pixel - 1) = color2;
  *(pixel + 1) = color2;
  *(pixel - stride_in_pixels) = color2;
  *(pixel + stride_in_pixels) = color2;
}

// Superimposes dots on the positions.
void Voronoi::SuperimposePositions() {
  const uint32_t white = MakeBGRA(255, 255, 255, 255);
  const uint32_t gray = MakeBGRA(192, 192, 192, 255);
  for (int i = 0; i < point_count_; i++) {
    RenderDot(
        screen_positions_[i].x, screen_positions_[i].y, white, gray);
  }
}

// Renders the Voronoi diagram, dispatching the work to multiple threads.
void Voronoi::Render() {
  workers_->Dispatch(num_regions_, wRenderRegionEntry, this);
  if (draw_points_)
    SuperimposePositions();
}

void Voronoi::StartBenchmark() {
  Reset();
  printf("Benchmark started...\n");
  benchmark_frame_counter_ = kFramesToBenchmark;
  benchmarking_ = true;
  benchmark_start_time_ = getseconds();
}

void Voronoi::EndBenchmark() {
  benchmark_end_time_ = getseconds();
  printf("Benchmark ended... time: %2.5f\n",
      benchmark_end_time_ - benchmark_start_time_);
  benchmarking_ = false;
  benchmark_frame_counter_ = 0;
  double result = benchmark_end_time_ - benchmark_start_time_;
  PostUpdateMessage("benchmark_result", result);
}

// Handle input events from the user and messages from JS.
void Voronoi::HandleEvent(PSEvent* ps_event) {
  // Give the 2D context a chance to process the event.
  if (0 != PSContext2DHandleEvent(ps_context_, ps_event))
    return;
  if (ps_event->type == PSE_INSTANCE_HANDLEINPUT) {
    // Convert Pepper Simple event to a PPAPI C++ event
    pp::InputEvent event(ps_event->as_resource);
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_KEYDOWN: {
        pp::KeyboardInputEvent key = pp::KeyboardInputEvent(event);
        uint32_t key_code = key.GetKeyCode();
        if (key_code == 84)  // 't' key
          if (!benchmarking_)
            StartBenchmark();
        break;
      }
      case PP_INPUTEVENT_TYPE_TOUCHSTART:
      case PP_INPUTEVENT_TYPE_TOUCHMOVE: {
        pp::TouchInputEvent touches = pp::TouchInputEvent(event);
        uint32_t count = touches.GetTouchCount(PP_TOUCHLIST_TYPE_TOUCHES);
        // Touch points 0..n directly set position of points 0..n in
        // Voronoi diagram.
        for (uint32_t i = 0; i < count; i++) {
          pp::TouchPoint touch =
              touches.GetTouchByIndex(PP_TOUCHLIST_TYPE_TOUCHES, i);
          pp::FloatPoint point = touch.position();
          positions_[i].Set(point.x() / ps_context_->width,
                            point.y() / ps_context_->height);
        }
        break;
      }
      default:
        break;
    }
  } else if (ps_event->type == PSE_INSTANCE_HANDLEMESSAGE) {
    // Convert Pepper Simple message to PPAPI C++ var
    pp::Var var(ps_event->as_var);
    if (var.is_dictionary()) {
      pp::VarDictionary dictionary(var);
      std::string message = dictionary.Get("message").AsString();
      if (message == "run_benchmark" && !benchmarking_)
       StartBenchmark();
      else if (message == "draw_points")
        draw_points_ = dictionary.Get("value").AsBool();
      else if (message == "draw_interiors")
        draw_interiors_ = dictionary.Get("value").AsBool();
      else if (message == "set_points") {
        int num_points = dictionary.Get("value").AsInt();
        point_count_ = std::min(kMaxPointCount, std::max(0, num_points));
      } else if (message == "set_threads") {
        int thread_count = dictionary.Get("value").AsInt();
        delete workers_;
        workers_ = new ThreadPool(thread_count);
      }
    }
  }
}

// PostUpdateMessage() helper function for sendimg small messages to JS.
void Voronoi::PostUpdateMessage(const char* message_name, double value) {
  pp::VarDictionary message;
  message.Set("message", message_name);
  message.Set("value", value);
  PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), message.pp_var());
}

void Voronoi::Update() {
  PSContext2DGetBuffer(ps_context_);
  if (NULL == ps_context_->data)
    return;
  assert(is_pow2(ps_context_->width));
  assert(is_pow2(ps_context_->height));

  // When benchmarking is running, don't update display via
  // PSContext2DSwapBuffer() - vsync is enabled by default, and will throttle
  // the benchmark results.
  do {
    UpdateSim();
    Render();
    if (!benchmarking_) break;
    --benchmark_frame_counter_;
  } while (benchmark_frame_counter_ > 0);
  if (benchmarking_)
    EndBenchmark();

  PSContext2DSwapBuffer(ps_context_);
}

// Starting point for the module.
int main(int argc, char* argv[]) {
  Voronoi voronoi;
  while (true) {
    PSEvent* ps_event;
    // Consume all available events.
    while ((ps_event = PSEventTryAcquire()) != NULL) {
      voronoi.HandleEvent(ps_event);
      PSEventRelease(ps_event);
    }
    // Do simulation, render and present.
    voronoi.Update();
  }

  return 0;
}
