// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_

#include <cstdint>

#include "base/containers/span.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {
struct SwapBuffersCompleteParams;

class GpuControlClient {
 public:
  // Informs the client that the context was lost. It should inform its own
  // clients or take actions as needed. This will only be called a single time
  // for any GpuControl.
  virtual void OnGpuControlLostContext() = 0;
  // This may happen inside calls from the client to the GpuControl, so this
  // function is reentrant. It informs the client of loss, but the client will
  // also receive a OnGpuControlLostContext (non-re-entrantly) in the future.
  // Use this only to update internal state if needed to make lost context be
  // visible immediately while unwinding the call stack.
  virtual void OnGpuControlLostContextMaybeReentrant() = 0;
  virtual void OnGpuControlErrorMessage(const char* message, int32_t id) = 0;
  virtual void OnGpuControlSwapBuffersCompleted(
      const SwapBuffersCompleteParams& params) = 0;
  virtual void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {}
  virtual void OnSwapBufferPresented(
      uint64_t swap_id,
      const gfx::PresentationFeedback& feedback) = 0;
  // Sent by the WebGPUDecoder
  virtual void OnGpuControlReturnData(base::span<const uint8_t> data) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_
