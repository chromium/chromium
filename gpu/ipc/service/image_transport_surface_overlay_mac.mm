// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

#include <dawn/native/MetalBackend.h>
#include <dawn/webgpu_cpp.h>

#include <memory>
#include <sstream>

#include "base/command_line.h"
#include "base/cpu_reduction_experiment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gl/ca_renderer_layer_params.h"

// From ANGLE's EGL/eglext_angle.h. This should be included instead of being
// redefined here.
#ifndef EGL_ANGLE_device_metal
#define EGL_ANGLE_device_metal 1
#define EGL_METAL_DEVICE_ANGLE 0x34A6
#endif /* EGL_ANGLE_device_metal */

namespace gpu {

namespace {
constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
constexpr int kHistogramTimeBuckets = 50;

// Control use of AVFoundation to draw video content.
BASE_FEATURE(kAVFoundationOverlays,
             "avfoundation-overlays",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Use CVDisplayLink timing for PresentationFeedback timestamps.
BASE_FEATURE(kNewPresentationFeedbackTimeStamps,
             "NewPresentationFeedbackTimeStamps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Record the delay from the system CVDisplayLink or CADisplaylink source to
// CrGpuMain OnVSyncPresentation().
void RecordVSyncCallbackDelay(base::TimeDelta delay) {
  if (!base::ShouldLogHistogramForCpuReductionExperiment()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "GPU.Presentation.VSyncCallbackDelay", delay,
      /*min=*/base::Microseconds(10),
      /*max=*/base::Milliseconds(33), /*bucket_count=*/50);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

ImageTransportSurfaceOverlayMacEGL::ImageTransportSurfaceOverlayMacEGL(
    DawnContextProvider* dawn_context_provider)
    : dawn_context_provider_(dawn_context_provider), weak_ptr_factory_(this) {
  static bool av_disabled_at_command_line =
      !base::FeatureList::IsEnabled(kAVFoundationOverlays);

  auto buffer_presented_callback =
      base::BindRepeating(&ImageTransportSurfaceOverlayMacEGL::BufferPresented,
                          weak_ptr_factory_.GetWeakPtr());
  bool use_new_presentation_timestamps = false;
#if BUILDFLAG(IS_MAC)
  use_new_presentation_timestamps =
      base::FeatureList::IsEnabled(kNewPresentationFeedbackTimeStamps);
#endif
  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      !av_disabled_at_command_line, use_new_presentation_timestamps,
      std::move(buffer_presented_callback));
}

ImageTransportSurfaceOverlayMacEGL::~ImageTransportSurfaceOverlayMacEGL() {
  ca_layer_tree_coordinator_.reset();
}

void ImageTransportSurfaceOverlayMacEGL::BufferPresented(
    PresentationCallback callback,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(feedback);
}

void ImageTransportSurfaceOverlayMacEGL::Present(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::Present");
  ca_layer_tree_coordinator_->SetCALayerErrorCode(data.ca_layer_error_code);

  // Commit the first pending frame before adding one more in Present() if there
  // are more than supported .
  if (ca_layer_tree_coordinator_->NumPendingSwaps() >= cap_max_pending_swaps_) {
    CommitPresentedFrameToCA();
  }

  // Set the display HDR headroom to be used for any tone mapping to be done
  // at the CoreAnimation level.
  ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->SetDisplayHDRHeadroom(data.display_hdr_headroom);

  // Query the underlying Metal device, if one exists. This is needed to ensure
  // synchronization between the display compositor and the HDRCopierLayer.
  // https://crbug.com/1372898
  if (gl::GLDisplayEGL* display =
          gl::GLDisplayEGL::GetDisplayForCurrentContext()) {
    // With SkiaGraphite, we pass the Graphite-Dawn MTLDevice for creating
    // CAMetalLayer used to display HDR IOSurfaces. With SkiaGanesh, we pass the
    // ANGLE MTLDevice instead.
    if (dawn_context_provider_ &&
        dawn_context_provider_->backend_type() == wgpu::BackendType::Metal) {
      id<MTLDevice> metal_device = dawn::native::metal::GetMTLDevice(
          dawn_context_provider_->GetDevice().Get());
      ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
          ->SetMetalDevice(metal_device);
    } else {
      EGLAttrib angle_device_attrib = 0;
      if (eglQueryDisplayAttribEXT(display->GetDisplay(), EGL_DEVICE_EXT,
                                   &angle_device_attrib)) {
        EGLDeviceEXT angle_device =
            reinterpret_cast<EGLDeviceEXT>(angle_device_attrib);
        EGLAttrib metal_device_attrib = 0;
        if (eglQueryDeviceAttribEXT(angle_device, EGL_METAL_DEVICE_ANGLE,
                                    &metal_device_attrib)) {
          id<MTLDevice> metal_device = (__bridge id)(void*)metal_device_attrib;
          ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
              ->SetMetalDevice(metal_device);
        }
      }
    }
  }

  ca_layer_tree_coordinator_->Present(std::move(completion_callback),
                                      std::move(presentation_callback));

#if BUILDFLAG(IS_MAC)
  if (display_link_mac_ && !vsync_callback_mac_) {
    vsync_callback_mac_ =
        display_link_mac_->RegisterCallback(base::BindRepeating(
            &ImageTransportSurfaceOverlayMacEGL::OnVSyncPresentation,
            weak_ptr_factory_.GetWeakPtr()));
  }

  bool delay_presenetation_until_next_vsync =
      features::IsVSyncAlignedPresentEnabled();

  if (vsync_callback_mac_) {
    vsync_callback_mac_keep_alive_counter_ = kMaxKeepAliveCounter;
    if (delay_presenetation_until_next_vsync) {
      // Delay CommitPresentedFrameToCA() until OnVSyncPresentation().
      return;
    }
  }
#endif

  CommitPresentedFrameToCA();
}

void ImageTransportSurfaceOverlayMacEGL::CommitPresentedFrameToCA() {
  //  Do a GL fence for flush to apply back-pressure before drawing.
  {
    base::TimeTicks start_time = base::TimeTicks::Now();
    ca_layer_tree_coordinator_->ApplyBackpressure();
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Gpu.Mac.BackpressureUs", base::TimeTicks::Now() - start_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Update the CALayer tree in the GPU process.
  {
    base::TimeTicks before_transaction_time = base::TimeTicks::Now();
    TRACE_EVENT0("gpu", "CommitPresentedFrameToCA");
    base::TimeTicks display_time;
    base::TimeDelta frame_interval;
#if BUILDFLAG(IS_MAC)
    display_time = GetDisplaytime(base::TimeTicks::Now());
    frame_interval = frame_interval_;
#endif
    ca_layer_tree_coordinator_->CommitPresentedFrameToCA(frame_interval,
                                                         display_time);

    base::TimeDelta transaction_time =
        base::TimeTicks::Now() - before_transaction_time;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.IOSurface.CATransactionTimeUs", transaction_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }
}

bool ImageTransportSurfaceOverlayMacEGL::ScheduleOverlayPlane(
    gl::OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (absl::get<gfx::OverlayTransform>(overlay_plane_data.plane_transform) !=
      gfx::OVERLAY_TRANSFORM_NONE) {
    DLOG(ERROR) << "Invalid overlay plane transform.";
    return false;
  }
  if (overlay_plane_data.z_order) {
    DLOG(ERROR) << "Invalid non-zero Z order.";
    return false;
  }
  // TODO(crbug.com/40818047): the display_bounds might not need to be rounded
  // to the nearest rect as this eventually gets made into a CALayer. CALayers
  // work in floats.
  const ui::CARendererLayerParams overlay_as_calayer_params(
      /*is_clipped=*/false,
      /*clip_rect=*/gfx::Rect(),
      /*rounded_corner_bounds=*/gfx::RRectF(),
      /*sorting_context_id=*/0, gfx::Transform(), image,
      overlay_plane_data.color_space,
      /*contents_rect=*/overlay_plane_data.crop_rect,
      /*rect=*/gfx::ToNearestRect(overlay_plane_data.display_bounds),
      /*background_color=*/SkColors::kTransparent,
      /*edge_aa_mask=*/0,
      /*opacity=*/1.f,
      /*nearest_neighbor_filter=*/GL_LINEAR,
      /*hdr_metadata=*/gfx::HDRMetadata(),
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear,
      /*is_render_pass_draw_quad=*/false);

  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(overlay_as_calayer_params);
}

bool ImageTransportSurfaceOverlayMacEGL::ScheduleCALayer(
    const ui::CARendererLayerParams& params) {
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(params);
}

bool ImageTransportSurfaceOverlayMacEGL::Resize(
    const gfx::Size& pixel_size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
  return true;
}

void ImageTransportSurfaceOverlayMacEGL::SetMaxPendingSwaps(
    int max_pending_swaps) {
#if BUILDFLAG(IS_MAC)
  cap_max_pending_swaps_ =
      std::min(max_pending_swaps, features::NumPendingFrameSupported());
  // MaxCALayerTrees is equal to the number of max_pending_swaps + one
  // that has been displayed.
  ca_layer_tree_coordinator_->SetMaxCALayerTrees(cap_max_pending_swaps_ + 1);
#endif
}

#if BUILDFLAG(IS_MAC)
void ImageTransportSurfaceOverlayMacEGL::SetVSyncDisplayID(int64_t display_id) {
  if (!features::IsVSyncAlignedPresentEnabled() &&
      !base::FeatureList::IsEnabled(kNewPresentationFeedbackTimeStamps)) {
    return;
  }

  if ((!display_link_mac_ || display_id != display_id_) &&
      display_id != display::kInvalidDisplayId) {
    vsync_callback_mac_ = nullptr;

    // Commit all pending frames before switching to the new monitor.
    while (ca_layer_tree_coordinator_->NumPendingSwaps()) {
      vsync_callback_mac_keep_alive_counter_ =
          std::max(vsync_callback_mac_keep_alive_counter_, 1);
      OnVSyncPresentation(ui::VSyncParamsMac());
    }

    display_link_mac_ = ui::DisplayLinkMac::GetForDisplay(display_id);
  }
  display_id_ = display_id;
}

base::TimeTicks ImageTransportSurfaceOverlayMacEGL::GetDisplaytime(
    base::TimeTicks latch_time) {
  // From the CVDisplayLink params dump:
  // |next_display_time_| ~= |current_display_time_| + |frame_interval|.
  // params.display_time ~= params.callback_time + 1.5x |frame_interval|.

  // From the experiment, frames committed before (|current_display_time_| - 1.5
  // ms) will be displayed at the next display time. 1.5 ms is roughly the safe
  // zone for the latch deadline. The result is inconsistent in the experiment
  // if commit is too close to the display_time.
  constexpr base::TimeDelta kLatchBufferTime = base::Microseconds(1500);
  auto latch_deadline_for_next_display =
      current_display_time_ - kLatchBufferTime;
  if (latch_time < latch_deadline_for_next_display) {
    return next_display_time_;
  }

  // We just missed the |current_display_time|, the display will be at the next
  // one after |next_display_time_|.
  if (!frame_interval_.is_zero() && next_display_time_ != base::TimeTicks()) {
    base::TimeTicks present_time =
        latch_time.SnappedToNextTick(next_display_time_ - kLatchBufferTime,
                                     frame_interval_) +
        kLatchBufferTime + frame_interval_;
    return present_time;
  }

  // When there is no display_time info, just use the latch_time.
  // This only happens at the very first frame after the browser starts,
  return latch_time;
}

// The CVDisplayLink callback on the GPU thread.
void ImageTransportSurfaceOverlayMacEGL::OnVSyncPresentation(
    ui::VSyncParamsMac params) {
  // Documentation for the CVDisplayLink display_time
  // https://developer.apple.com/documentation/corevideo/cvdisplaylinkoutputcallback

  current_display_time_ = next_display_time_;

  if (params.display_times_valid) {
    next_display_time_ = params.display_timebase;
    frame_interval_ = params.display_interval;
  }

  if (params.callback_times_valid) {
    RecordVSyncCallbackDelay(base::TimeTicks::Now() - params.callback_timebase);
  }

  if (ca_layer_tree_coordinator_->NumPendingSwaps()) {
    CommitPresentedFrameToCA();
  }

  vsync_callback_mac_keep_alive_counter_--;

  if (vsync_callback_mac_keep_alive_counter_ == 0) {
    vsync_callback_mac_ = nullptr;
  }
}
#endif

}  // namespace gpu
