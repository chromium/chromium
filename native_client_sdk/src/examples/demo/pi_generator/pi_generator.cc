// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_context_2d.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_interface.h"

#ifdef WIN32
#undef PostMessage
#endif

namespace {

const int kMaxPointCount = 1000000000;  // The total number of points to draw.
const double kSecsPerFrame = 0.005;  // How long to draw points before swapping.
const uint32_t kOpaqueColorMask = 0xff000000;  // Opaque pixels.
const uint32_t kRedMask = 0xff0000;
const uint32_t kBlueMask = 0xff;
const uint32_t kRedShift = 16;
const uint32_t kBlueShift = 0;

int g_points_in_circle = 0;
int g_total_points = 0;
double g_pi = 0;

}  // namespace

bool Render(PSContext2D_t* ctx) {
  PSContext2DGetBuffer(ctx);

  if (NULL == ctx->data)
    return true;

  PP_TimeTicks start_time = PSInterfaceCore()->GetTimeTicks();
  while (PSInterfaceCore()->GetTimeTicks() - start_time < kSecsPerFrame) {
    double x = static_cast<double>(rand()) / RAND_MAX;
    double y = static_cast<double>(rand()) / RAND_MAX;
    double distance = sqrt(x * x + y * y);
    int px = x * ctx->width;
    int py = (1.0 - y) * ctx->height;
    uint32_t color = ctx->data[ctx->width * py + px];

    ++g_total_points;
    if (distance < 1.0) {
      ++g_points_in_circle;
      g_pi = 4.0 * g_points_in_circle / g_total_points;
      // Set color to blue.
      color += 4 << kBlueShift;
      color &= kBlueMask;
    } else {
      // Set color to red.
      color += 4 << kRedShift;
      color &= kRedMask;
    }
    ctx->data[ctx->width * py + px] = color | kOpaqueColorMask;
  }

  PSContext2DSwapBuffer(ctx);
  return g_total_points != kMaxPointCount;
}

/*
 * Starting point for the module.  We do not use main since it would
 * collide with main in libppapi_cpp.
 */
int main(int argc, char* argv[]) {
  unsigned int seed = 1;
  srand(seed);

  PSEventSetFilter(PSE_ALL);

  PSContext2D_t* ctx = PSContext2DAllocate(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  bool running = true;
  while (running) {
    PSEvent* event;

    // Consume all available events
    while ((event = PSEventTryAcquire()) != NULL) {
      PSContext2DHandleEvent(ctx, event);
      PSEventRelease(event);
    }

    if (ctx->bound) {
      running = Render(ctx);
    }

    // Send the current PI value to JavaScript.
    PP_Var var = PP_MakeDouble(g_pi);
    PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), var);
  }

  return 0;
}
