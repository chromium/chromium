// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HEURISTIC_PARAMETERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HEURISTIC_PARAMETERS_H_

#include "build/build_config.h"

namespace blink {

namespace canvas_heuristic_parameters {

enum {
  // Disable Deferral overdraw parameters
  //======================================

  // Heuristic: Canvases that are overdrawn beyond this factor in a
  // single frame will be disabled deferral.
  kExpensiveOverdrawThreshold = 10,

  // GPU readback prevention heuristics
  //====================================

  // When a canvas is used as a source image, if its destination is
  // non-accelerated and the source canvas is accelerated, a readback
  // from the gpu is necessary. This option causes the source canvas to
  // switch to non-accelerated when this situation is encountered to
  // prevent future canvas-to-canvas draws from requiring a readback.
  kDisableAccelerationToAvoidReadbacks = 0,

  // See description of DisableAccelerationToAvoidReadbacks. This is the
  // opposite strategy : accelerate the destination canvas. If both
  // EnableAccelerationToAvoidReadbacks and
  // DisableAccelerationToAvoidReadbacks are specified, we try to enable
  // acceleration on the destination first. If that does not succeed,
  // we disable acceleration on the source canvas. Either way, future
  // readbacks are prevented.
  kEnableAccelerationToAvoidReadbacks = 1,

  // Accelerated rendering heuristics
  // =================================

  // Enables frequent flushing of the GrContext for accelerated canvas. Since
  // skia internally batches the GrOp list when flushing the recording onto the
  // SkCanvasand may only flush it the command buffer at the end of the frame,
  // it can lead to inefficient parallelization with the GPU. This enables
  // triggering context flushes at regular intervals, after a fixed number of
  // draws.
  kEnableGrContextFlushes = 1,

  // The maximum number of draw ops executed on the canvas, after which the
  // underlying GrContext is flushed.
  kMaxDrawsBeforeContextFlush = 50,

  // Canvas resource provider
  // ========================

  // The maximum number of inflight resources waiting to be used for recycling.
  kMaxRecycledCanvasResources = 2,
};  // enum

}  // namespace canvas_heuristic_parameters

}  // namespace blink

#endif
