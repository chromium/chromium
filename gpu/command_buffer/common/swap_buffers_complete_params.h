// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_
#define GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_

#include <vector>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

struct GPU_EXPORT SwapBuffersCompleteParams {
  SwapBuffersCompleteParams();
  SwapBuffersCompleteParams(SwapBuffersCompleteParams&& other);
  SwapBuffersCompleteParams(const SwapBuffersCompleteParams& other);
  SwapBuffersCompleteParams& operator=(SwapBuffersCompleteParams&& other);
  SwapBuffersCompleteParams& operator=(const SwapBuffersCompleteParams& other);
  ~SwapBuffersCompleteParams();

  gfx::SwapResponse swap_response;

  // Damage area of the current backing buffer compare to the previous swapped
  // buffer. The renderer can use it as hint for minimizing drawing area for the
  // next frame.
  absl::optional<gfx::Rect> frame_buffer_damage_area;

  // The mailbox corresponding to the primary plane that was just swapped to
  // the front buffer. The overlay processor can use it to extract the buffer
  // for page flip tests.
  Mailbox primary_plane_mailbox;

  // Used only on macOS, to allow the browser hosted NSWindow to display
  // content populated in the GPU process.
  gfx::CALayerParams ca_layer_params;

  // Used only on macOS, for released overlays with SkiaRenderer.
  std::vector<Mailbox> released_overlays;

  // Used by graphics pipeline to trace each individual frame swap. The value is
  // passed from viz::Display::DrawAndSwap to Renderer, then to gl::Presenter or
  // gl::GLSurface via gfx::FrameData and then passed back to viz::Display via
  // gfx::SwapCompletionResult.
  int64_t swap_trace_id = -1;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_
