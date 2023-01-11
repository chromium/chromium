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
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffers(
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     local_swap_id_),
      data);
  response.result = result;
  FinishSwapBuffers(std::move(response), local_swap_id_,
                    /*release_fence=*/gfx::GpuFenceHandle());
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);

  // We use WeakPtr here to avoid manual management of life time of an instance
  // of this class. Callback will not be called once the instance of this class
  // is destroyed. However, this also means that the callback can be run on
  // the calling thread only.
  gl::GLSurfaceAdapter::SwapBuffersAsync(
      base::BindOnce(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), response, local_swap_id_),
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), local_swap_id_),
      data);
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects,
    PresentationCallback callback,
    gfx::FrameData data) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffersWithBounds(
      rects,
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     local_swap_id_),
      data);
  response.result = result;
  FinishSwapBuffers(response, local_swap_id_,
                    /*release_fence=*/gfx::GpuFenceHandle());
  return result;
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback,
    gfx::FrameData data) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gfx::SwapResult result = gl::GLSurfaceAdapter::PostSubBuffer(
      x, y, width, height,
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     local_swap_id_),
      data);
  response.result = result;
  FinishSwapBuffers(response, local_swap_id_,
                    /*release_fence=*/gfx::GpuFenceHandle());

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
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gl::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::BindOnce(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), response, local_swap_id_),
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), local_swap_id_),
      data);
}

gfx::SwapResult PassThroughImageTransportSurface::CommitOverlayPlanes(
    PresentationCallback callback,
    gfx::FrameData data) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gfx::SwapResult result = gl::GLSurfaceAdapter::CommitOverlayPlanes(
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     local_swap_id_),
      data);
  response.result = result;
  FinishSwapBuffers(response, local_swap_id_,
                    /*release_fence=*/gfx::GpuFenceHandle());
  return result;
}

void PassThroughImageTransportSurface::CommitOverlayPlanesAsync(
    SwapCompletionCallback callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gl::GLSurfaceAdapter::CommitOverlayPlanesAsync(
      base::BindOnce(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     response, local_swap_id_),
      base::BindOnce(&PassThroughImageTransportSurface::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), local_swap_id_),
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

void PassThroughImageTransportSurface::StartSwapBuffers(
    gfx::SwapResponse* response) {
  TrackMultiSurfaceSwap();
  UpdateVSyncEnabled();

#if DCHECK_IS_ON()
  // Store the local swap id to ensure the presentation callback is not called
  // before this swap is completed.
  pending_local_swap_ids_.push(++local_swap_id_);
#endif
  // Correct id will be populated later in the DecoderClient, before passing to
  // client.
  response->swap_id = 0;

  response->timings.swap_start = base::TimeTicks::Now();
}

void PassThroughImageTransportSurface::FinishSwapBuffers(
    gfx::SwapResponse response,
    uint64_t local_swap_id,
    gfx::GpuFenceHandle release_fence) {
  response.timings.swap_end = base::TimeTicks::Now();

#if DCHECK_IS_ON()
  // After the swap is completed, the local swap id is removed from the queue,
  // and the presentation callback for this swap can be run at any time later.
  DCHECK_EQ(pending_local_swap_ids_.front(), local_swap_id);
  pending_local_swap_ids_.pop();
#endif

  if (delegate_) {
    auto blocked_time_since_last_swap =
        delegate_->GetGpuBlockedTimeSinceLastSwap();

    if (!multiple_surfaces_swapped_) {
      static constexpr base::TimeDelta kTimingMetricsHistogramMin =
          base::Microseconds(5);
      static constexpr base::TimeDelta kTimingMetricsHistogramMax =
          base::Milliseconds(500);
      static constexpr uint32_t kTimingMetricsHistogramBuckets = 50;

      base::TimeDelta delta =
          response.timings.swap_end - response.timings.swap_start;
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "GPU.SwapTimeUs", delta, kTimingMetricsHistogramMin,
          kTimingMetricsHistogramMax, kTimingMetricsHistogramBuckets);

      // Report only if collection is enabled and supported on current platform
      // See gpu::Scheduler::TakeTotalBlockingTime for details.
      if (!blocked_time_since_last_swap.is_min()) {
        LOCAL_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            "GPU.GpuBlockedBetweenSwapsUs2", blocked_time_since_last_swap,
            kTimingMetricsHistogramMin, kTimingMetricsHistogramMax,
            kTimingMetricsHistogramBuckets);
      }
    }
  }
}

void PassThroughImageTransportSurface::FinishSwapBuffersAsync(
    SwapCompletionCallback callback,
    gfx::SwapResponse response,
    uint64_t local_swap_id,
    gfx::SwapCompletionResult result) {
  response.result = result.swap_result;
  FinishSwapBuffers(response, local_swap_id, result.release_fence.Clone());
  std::move(callback).Run(std::move(result));
}

void PassThroughImageTransportSurface::BufferPresented(
    GLSurface::PresentationCallback callback,
    uint64_t local_swap_id,
    const gfx::PresentationFeedback& feedback) {
#if DCHECK_IS_ON()
  // The swaps are handled in queue. Thus, to allow the presentation feedback to
  // be called after the first swap ack later, disregarding any of the following
  // swap requests with own presentation feedbacks, and disallow calling the
  // presentation callback before the same swap request, make sure the queue is
  // either empty or the pending swap id is greater than the current. This means
  // that the requested swap is completed and it's safe to call the presentation
  // callback.
  DCHECK(pending_local_swap_ids_.empty() ||
         pending_local_swap_ids_.front() > local_swap_id);
#endif

  std::move(callback).Run(feedback);
}

}  // namespace gpu
