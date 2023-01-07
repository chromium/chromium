// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <ppapi/c/ppb_input_event.h>
#include <ppapi/cpp/fullscreen.h>
#include <ppapi/cpp/input_event.h>
#include <ppapi/cpp/instance_handle.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_context_2d.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_interface.h"
#include "sdk_util/macros.h"
#include "sdk_util/thread_pool.h"

using namespace sdk_util;  // For sdk_util::ThreadPool

namespace {

#define INLINE inline __attribute__((always_inline))

// BGRA helper macro, for constructing a pixel for a BGRA buffer.
#define MakeBGRA(b, g, r, a)  \
  (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

const int kFramesToBenchmark = 100;
const int kCellAlignment = 0x10;

// 128 bit vector types
typedef uint8_t u8x16_t __attribute__ ((vector_size (16)));

// Helper function to broadcast x across 16 element vector.
INLINE u8x16_t broadcast(uint8_t x) {
  u8x16_t r = {x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x};
  return r;
}

// Convert a count value into a live (green) or dead color value.
const uint32_t kNeighborColors[] = {
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
};

// These represent the new health value of a cell based on its neighboring
// values.  The health is binary: either alive or dead.
const uint8_t kIsAlive[] = {
      0, 0, 0, 0, 0, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Timer helper for benchmarking.  Returns seconds elapsed since program start,
// as a double.
timeval start_tv;
int start_tv_retv = gettimeofday(&start_tv, NULL);

inline double getseconds() {
  const double usec_to_sec = 0.000001;
  timeval tv;
  if ((0 == start_tv_retv) && (0 == gettimeofday(&tv, NULL)))
    return (tv.tv_sec - start_tv.tv_sec) + tv.tv_usec * usec_to_sec;
  return 0.0;
}
} // namespace


class Life {
 public:
  Life();
  virtual ~Life();
  // Runs a tick of the simulations, update 2D output.
  void Update();
  // Handle event from user, or message from JS.
  void HandleEvent(PSEvent* ps_event);
 private:
  void UpdateContext();
  void DrawCell(int32_t x, int32_t y);
  void ProcessTouchEvent(const pp::TouchInputEvent& touches);
  void PostUpdateMessage(const char* message, double value);
  void StartBenchmark();
  void EndBenchmark();
  void Stir();
  void wSimulate(int y);
  static void wSimulateEntry(int y, void* data);
  void Simulate();

  bool simd_;
  bool multithread_;
  bool benchmarking_;
  int benchmark_frame_counter_;
  double bench_start_time_;
  double bench_end_time_;
  uint8_t* cell_in_;
  uint8_t* cell_out_;
  int32_t cell_stride_;
  int32_t width_;
  int32_t height_;
  PSContext2D_t* ps_context_;
  ThreadPool* workers_;
};

Life::Life() :
    simd_(true),
    multithread_(true),
    benchmarking_(false),
    benchmark_frame_counter_(0),
    bench_start_time_(0.0),
    bench_end_time_(0.0),
    cell_in_(NULL),
    cell_out_(NULL),
    cell_stride_(0),
    width_(0),
    height_(0) {
  ps_context_ = PSContext2DAllocate(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  // Query system for number of processors via sysconf()
  int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_threads < 2)
    num_threads = 2;
  workers_ = new ThreadPool(num_threads);
  PSEventSetFilter(PSE_ALL);
}

Life::~Life() {
  delete workers_;
  PSContext2DFree(ps_context_);
}

void Life::UpdateContext() {
  cell_stride_ = (ps_context_->width + kCellAlignment - 1) &
      ~(kCellAlignment - 1);
  size_t size = cell_stride_ * ps_context_->height;

  if (ps_context_->width != width_ || ps_context_->height != height_) {
    free(cell_in_);
    free(cell_out_);

    // Create a new context
    void* in_buffer = NULL;
    void* out_buffer = NULL;
    // alloc buffers aligned on 16 bytes
    posix_memalign(&in_buffer, kCellAlignment, size);
    posix_memalign(&out_buffer, kCellAlignment, size);
    cell_in_ = (uint8_t*) in_buffer;
    cell_out_ = (uint8_t*) out_buffer;

    memset(cell_out_, 0, size);
    for (size_t index = 0; index < size; index++) {
      cell_in_[index] = rand() & 1;
    }
    width_ = ps_context_->width;
    height_ = ps_context_->height;
  }
}

void Life::DrawCell(int32_t x, int32_t y) {
  if (!cell_in_) return;
  if (x > 0 && x < ps_context_->width - 1 &&
      y > 0 && y < ps_context_->height - 1) {
    cell_in_[x - 1 + y * cell_stride_] = 1;
    cell_in_[x + 1 + y * cell_stride_] = 1;
    cell_in_[x + (y - 1) * cell_stride_] = 1;
    cell_in_[x + (y + 1) * cell_stride_] = 1;
  }
}

void Life::ProcessTouchEvent(const pp::TouchInputEvent& touches) {
  uint32_t count = touches.GetTouchCount(PP_TOUCHLIST_TYPE_TOUCHES);
  uint32_t i, j;
  for (i = 0; i < count; i++) {
    pp::TouchPoint touch =
        touches.GetTouchByIndex(PP_TOUCHLIST_TYPE_TOUCHES, i);
    int radius = (int)(touch.radii().x());
    int x = (int)(touch.position().x());
    int y = (int)(touch.position().y());
    // num = 1/100th the area of touch point
    uint32_t num = (uint32_t)(M_PI * radius * radius / 100.0f);
    for (j = 0; j < num; j++) {
      int dx = rand() % (radius * 2) - radius;
      int dy = rand() % (radius * 2) - radius;
      // only plot random cells within the touch area
      if (dx * dx + dy * dy <= radius * radius)
        DrawCell(x + dx, y + dy);
    }
  }
}

void Life::PostUpdateMessage(const char* message_name, double value) {
  pp::VarDictionary message;
  message.Set("message", message_name);
  message.Set("value", value);
  PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), message.pp_var());
}

void Life::StartBenchmark() {
  printf("Running benchmark... (SIMD: %s, multi-threading: %s, size: %dx%d)\n",
      simd_ ? "enabled" : "disabled",
      multithread_ ? "enabled" : "disabled",
      ps_context_->width,
      ps_context_->height);
  benchmarking_ = true;
  bench_start_time_ = getseconds();
  benchmark_frame_counter_ = kFramesToBenchmark;
}

void Life::EndBenchmark() {
  double total_time;
  bench_end_time_ = getseconds();
  benchmarking_ = false;
  total_time = bench_end_time_ - bench_start_time_;
  printf("Finished - benchmark took %f seconds\n", total_time);
  // Send benchmark result to JS.
  PostUpdateMessage("benchmark_result", total_time);
}

void Life::HandleEvent(PSEvent* ps_event) {
  // Give the 2D context a chance to process the event.
  if (0 != PSContext2DHandleEvent(ps_context_, ps_event)) {
    UpdateContext();
    return;
  }

  switch(ps_event->type) {

    case PSE_INSTANCE_HANDLEINPUT: {
      pp::InputEvent event(ps_event->as_resource);

      switch(event.GetType()) {
        case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
          pp::MouseInputEvent mouse = pp::MouseInputEvent(event);
          // If the button is down, draw
          if (mouse.GetModifiers() & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
            PP_Point location = mouse.GetPosition();
            DrawCell(location.x, location.y);
          }
          break;
        }

        case PP_INPUTEVENT_TYPE_TOUCHSTART:
        case PP_INPUTEVENT_TYPE_TOUCHMOVE: {
          pp::TouchInputEvent touches = pp::TouchInputEvent(event);
          ProcessTouchEvent(touches);
          break;
        }

        case PP_INPUTEVENT_TYPE_KEYDOWN: {
          pp::Fullscreen fullscreen((pp::InstanceHandle(PSGetInstanceId())));
          bool isFullscreen = fullscreen.IsFullscreen();
          fullscreen.SetFullscreen(!isFullscreen);
          break;
        }

        default:
          break;
      }
      break;  // case PSE_INSTANCE_HANDLEINPUT
    }

    case PSE_INSTANCE_HANDLEMESSAGE: {
      // Convert Pepper Simple message to PPAPI C++ vars
      pp::Var var(ps_event->as_var);
      if (var.is_dictionary()) {
        pp::VarDictionary dictionary(var);
        std::string message = dictionary.Get("message").AsString();
        if (message == "run_benchmark" && !benchmarking_) {
          StartBenchmark();
        } else if (message == "set_simd") {
          simd_ = dictionary.Get("value").AsBool();
        } else if (message == "set_threading") {
          multithread_ = dictionary.Get("value").AsBool();
        }
      }
      break;  // case PSE_INSTANCE_HANDLEMESSAGE
    }

    default:
      break;
  }
}

void Life::Stir() {
  int32_t width = ps_context_->width;
  int32_t height = ps_context_->height;
  int32_t stride = cell_stride_;
  int32_t i;
  if (cell_in_ == NULL || cell_out_ == NULL)
    return;

  for (i = 0; i < width; ++i) {
    cell_in_[i] = rand() & 1;
    cell_in_[i + (height - 1) * stride] = rand() & 1;
  }
  for (i = 0; i < height; ++i) {
    cell_in_[i * stride] = rand() & 1;
    cell_in_[i * stride + (width - 1)] = rand() & 1;
  }
}

void Life::wSimulate(int y) {
  // Don't run simulation on top and bottom borders
  if (y < 1 || y >= ps_context_->height - 1)
    return;

  // Do neighbor summation; apply rules, output pixel color. Note that a 1 cell
  // wide perimeter is excluded from the simulation update; only cells from
  // x = 1 to x < width - 1 and y = 1 to y < height - 1 are updated.
  uint8_t *src0 = (cell_in_ + (y - 1) * cell_stride_);
  uint8_t *src1 = src0 + cell_stride_;
  uint8_t *src2 = src1 + cell_stride_;
  uint8_t *dst = (cell_out_ + y * cell_stride_) + 1;
  uint32_t *pixels = static_cast<uint32_t *>(ps_context_->data);
  uint32_t *pixel_line = // static_cast<uint32_t*>
      (pixels + y * ps_context_->stride / sizeof(uint32_t));
  int32_t x = 1;

  if (simd_) {
    const u8x16_t kOne = broadcast(1);
    const u8x16_t kFour = broadcast(4);
    const u8x16_t kEight = broadcast(8);
    const u8x16_t kZero255 = {0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Prime the src
    u8x16_t src00 = *reinterpret_cast<u8x16_t*>(&src0[0]);
    u8x16_t src01 = *reinterpret_cast<u8x16_t*>(&src0[16]);
    u8x16_t src10 = *reinterpret_cast<u8x16_t*>(&src1[0]);
    u8x16_t src11 = *reinterpret_cast<u8x16_t*>(&src1[16]);
    u8x16_t src20 = *reinterpret_cast<u8x16_t*>(&src2[0]);
    u8x16_t src21 = *reinterpret_cast<u8x16_t*>(&src2[16]);

    // This inner loop is SIMD - each loop iteration will process 16 cells.
    for (; (x + 15) < (ps_context_->width - 1); x += 16) {

      // Construct jittered source temps, using __builtin_shufflevector(..) to
      // extract a shifted 16 element vector from the 32 element concatenation
      // of two source vectors.
      u8x16_t src0j0 = src00;
      u8x16_t src0j1 = __builtin_shufflevector(src00, src01,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src0j2 = __builtin_shufflevector(src00, src01,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
      u8x16_t src1j0 = src10;
      u8x16_t src1j1 = __builtin_shufflevector(src10, src11,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src1j2 = __builtin_shufflevector(src10, src11,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
      u8x16_t src2j0 = src20;
      u8x16_t src2j1 = __builtin_shufflevector(src20, src21,
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
      u8x16_t src2j2 = __builtin_shufflevector(src20, src21,
          2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);

      // Sum the jittered sources to construct neighbor count.
      u8x16_t count = src0j0 + src0j1 +  src0j2 +
                      src1j0 +        +  src1j2 +
                      src2j0 + src2j1 +  src2j2;
      // Add the center cell.
      count = count + count + src1j1;
      // If count > 4 and < 8, center cell will be alive in the next frame.
      u8x16_t alive1 = count > kFour;
      u8x16_t alive2 = count < kEight;
      // Intersect the two comparisons from above.
      u8x16_t alive = alive1 & alive2;

      // At this point, alive[x] will be one of two values:
      //   0x00 for a dead cell
      //   0xFF for an alive cell.
      //
      // Next, convert alive cells to green pixel color.
      // Use __builtin_shufflevector(..) to construct output pixels from
      // concantination of alive vector and kZero255 const vector.
      //   Indices 0..15 select the 16 cells from alive vector.
      //   Index 16 is zero constant from kZero255 constant vector.
      //   Index 17 is 255 constant from kZero255 constant vector.
      //   Output pixel color values are in BGRABGRABGRABGRA order.
      // Since each pixel needs 4 bytes of color information, 16 cells will
      // need to expand to 4 separate 16 byte pixel splats.
      u8x16_t pixel0_3 = __builtin_shufflevector(alive, kZero255,
        16, 0, 16, 17, 16, 1, 16, 17, 16, 2, 16, 17, 16, 3, 16, 17);
      u8x16_t pixel4_7 = __builtin_shufflevector(alive, kZero255,
        16, 4, 16, 17, 16, 5, 16, 17, 16, 6, 16, 17, 16, 7, 16, 17);
      u8x16_t pixel8_11 = __builtin_shufflevector(alive, kZero255,
        16, 8, 16, 17, 16, 9, 16, 17, 16, 10, 16, 17, 16, 11, 16, 17);
      u8x16_t pixel12_15 = __builtin_shufflevector(alive, kZero255,
        16, 12, 16, 17, 16, 13, 16, 17, 16, 14, 16, 17, 16, 15, 16, 17);

      // Write 16 pixels to output pixel buffer.
      *reinterpret_cast<u8x16_t*>(pixel_line + 0) = pixel0_3;
      *reinterpret_cast<u8x16_t*>(pixel_line + 4) = pixel4_7;
      *reinterpret_cast<u8x16_t*>(pixel_line + 8) = pixel8_11;
      *reinterpret_cast<u8x16_t*>(pixel_line + 12) = pixel12_15;

      // Convert alive mask to 1 or 0 and store in destination cell array.
      *reinterpret_cast<u8x16_t*>(dst) = alive & kOne;

      // Increment pointers.
      pixel_line += 16;
      dst += 16;
      src0 += 16;
      src1 += 16;
      src2 += 16;

      // Shift source over by 16 cells and read the next 16 cells.
      src00 = src01;
      src01 = *reinterpret_cast<u8x16_t*>(&src0[16]);
      src10 = src11;
      src11 = *reinterpret_cast<u8x16_t*>(&src1[16]);
      src20 = src21;
      src21 = *reinterpret_cast<u8x16_t*>(&src2[16]);
    }
  }

  // The SIMD loop above does 16 cells at a time.  The loop below is the
  // regular version which processes one cell at a time.  It is used to
  // finish the remainder of the scanline not handled by the SIMD loop.
  for (; x < (ps_context_->width - 1); ++x) {
    // Sum the jittered sources to construct neighbor count.
    int count = src0[0] + src0[1] + src0[2] +
                src1[0] +         + src1[2] +
                src2[0] + src2[1] + src2[2];
    // Add the center cell.
    count = count + count + src1[1];
    // Use table lookup indexed by count to determine pixel & alive state.
    uint32_t color = kNeighborColors[count];
    *pixel_line++ = color;
    *dst++ = kIsAlive[count];
    ++src0;
    ++src1;
    ++src2;
  }
}

// Static entry point for worker thread.
void Life::wSimulateEntry(int slice, void* thiz) {
  static_cast<Life*>(thiz)->wSimulate(slice);
}

void Life::Simulate() {
  // Stir up the edges to prevent the simulation from reaching steady state.
  Stir();

  if (multithread_) {
    // If multi-threading enabled, dispatch tasks to pool of worker threads.
    workers_->Dispatch(ps_context_->height, wSimulateEntry, this);
  } else {
    // Else manually simulate each line on this thread.
    for (int y = 0; y < ps_context_->height; y++) {
      wSimulateEntry(y, this);
    }
  }
  std::swap(cell_in_, cell_out_);
}

void Life::Update() {

  PSContext2DGetBuffer(ps_context_);
  if (NULL == ps_context_->data)
    return;

  // If we somehow have not allocated these pointers yet, skip this frame.
  if (!cell_in_ || !cell_out_) return;

  // Simulate one (or more if benchmarking) frames
  do {
    Simulate();
    if (!benchmarking_)
      break;
    --benchmark_frame_counter_;
  } while(benchmark_frame_counter_ > 0);
  if (benchmarking_)
    EndBenchmark();

 PSContext2DSwapBuffer(ps_context_);
}

// Starting point for the module.  We do not use main since it would
// collide with main in libppapi_cpp.
int main(int argc, char* argv[]) {
  Life life;
  while (true) {
    PSEvent* ps_event;
    // Consume all available events
    while ((ps_event = PSEventTryAcquire()) != NULL) {
      life.HandleEvent(ps_event);
      PSEventRelease(ps_event);
    }
    // Do simulation, render and present.
    life.Update();
  }
  return 0;
}
