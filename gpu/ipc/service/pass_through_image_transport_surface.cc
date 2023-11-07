// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/pass_through_image_transport_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"

namespace gpu {

namespace {
// Number of swap generations before vsync is reenabled after we've stopped
// doing multiple swaps per frame.
const int kMultiWindowSwapEnableVSyncDelay = 60;

int g_current_swap_generation_ = 0;
int g_num_swaps_in_current_swap_generation_ = 0;
int g_last_multi_window_swap_generation_ = 0;

}  // anonymous namespace

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    gl::GLSurface* surface,
    bool override_vsync_for_multi_window_swap)
    : GLSurfaceAdapter(surface),
      is_gpu_vsync_disabled_(!features::UseGpuVsync()),
      is_multi_window_swap_vsync_override_enabled_(
          override_vsync_for_multi_window_swap),
      delegate_(delegate) {}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() = default;

bool PassThroughImageTransportSurface::Initialize(gl::GLSurfaceFormat format) {
  // The surface is assumed to have already been initialized.
  return true;
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffers(
    PresentationCallback callback,
    gfx::FrameData data) {
  StartSwapBuffers();
  gfx::SwapResult result =
      gl::GLSurfaceAdapter::SwapBuffers(std::move(callback), data);
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  StartSwapBuffers();

  gl::GLSurfaceAdapter::SwapBuffersAsync(
      std::move(completion_callback), std::move(presentation_callback), data);
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects,
    PresentationCallback callback,
    gfx::FrameData data) {
  StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffersWithBounds(
      rects, std::move(callback), data);
  return result;
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback,
    gfx::FrameData data) {
  StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::PostSubBuffer(
      x, y, width, height, std::move(callback), data);

  return result;
}

void PassThroughImageTransportSurface::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  StartSwapBuffers();
  gl::GLSurfaceAdapter::PostSubBufferAsync(x, y, width, height,

                                           std::move(completion_callback),

                                           std::move(presentation_callback),
                                           data);
}

void PassThroughImageTransportSurface::SetVSyncEnabled(bool enabled) {
  if (vsync_enabled_ == enabled)
    return;
  vsync_enabled_ = enabled;
  GLSurfaceAdapter::SetVSyncEnabled(enabled);
}

void PassThroughImageTransportSurface::TrackMultiSurfaceSwap() {
  // This code is a simple way of enforcing that we only vsync if one surface
  // is swapping per frame. This provides single window cases a stable refresh
  // while allowing multi-window cases to not slow down due to multiple syncs
  // on a single thread. A better way to fix this problem would be to have
  // each surface present on its own thread.
  if (g_current_swap_generation_ == swap_generation_) {
    // No other surface has swapped since we swapped last time.
    if (g_num_swaps_in_current_swap_generation_ > 1)
      g_last_multi_window_swap_generation_ = g_current_swap_generation_;
    g_num_swaps_in_current_swap_generation_ = 0;
    g_current_swap_generation_++;
  }

  swap_generation_ = g_current_swap_generation_;
  g_num_swaps_in_current_swap_generation_++;

  multiple_surfaces_swapped_ =
      (g_num_swaps_in_current_swap_generation_ > 1) ||
      (g_current_swap_generation_ - g_last_multi_window_swap_generation_ <
       kMultiWindowSwapEnableVSyncDelay);
}

void PassThroughImageTransportSurface::UpdateVSyncEnabled() {
  if (is_gpu_vsync_disabled_) {
    SetVSyncEnabled(false);
    return;
  }

  bool should_override_vsync = false;
  if (is_multi_window_swap_vsync_override_enabled_) {
    should_override_vsync = multiple_surfaces_swapped_;
  }
  SetVSyncEnabled(!should_override_vsync);
}

void PassThroughImageTransportSurface::StartSwapBuffers() {
  TrackMultiSurfaceSwap();
  UpdateVSyncEnabled();
}

}  // namespace gpu
