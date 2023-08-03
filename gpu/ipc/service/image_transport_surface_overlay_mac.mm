// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

#include <memory>
#include <sstream>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_features.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/gl/gl_context.h"
#endif

// From ANGLE's EGL/eglext_angle.h. This should be included instead of being
// redefined here.
#ifndef EGL_ANGLE_device_metal
#define EGL_ANGLE_device_metal 1
#define EGL_METAL_DEVICE_ANGLE 0x34A6
#endif /* EGL_ANGLE_device_metal */

namespace gpu {

namespace {

// Control use of AVFoundation to draw video content.
BASE_FEATURE(kAVFoundationOverlays,
             "avfoundation-overlays",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

ImageTransportSurfaceOverlayMacEGL::ImageTransportSurfaceOverlayMacEGL(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : delegate_(delegate),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      weak_ptr_factory_(this) {
  static bool av_disabled_at_command_line =
      !base::FeatureList::IsEnabled(kAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer);

  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  if (use_remote_layer_api_) {
#if BUILDFLAG(IS_MAC)
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_ = [CAContext contextWithCGSConnection:connection_id
                                              options:@{}];
#else
    // Use a very large display ID to ensure that the context is never put
    // on-screen without being explicitly parented.
    ca_context_ = [CAContext remoteContextWithOptions:@{
      kCAContextIgnoresHitTest : @YES,
      kCAContextDisplayId : @10000
    }];
#endif
    ca_context_.layer = ca_layer_tree_coordinator_->GetCALayerForDisplay();
  }

#if BUILDFLAG(IS_MAC)
  if (features::UseGpuVsync()) {
    gpu_vsync_mac_ =
        std::make_unique<GpuVSyncMac>(delegate->GetGpuVSyncCallback());
  }
#endif
}

ImageTransportSurfaceOverlayMacEGL::~ImageTransportSurfaceOverlayMacEGL() {
  ca_layer_tree_coordinator_.reset();
}

void ImageTransportSurfaceOverlayMacEGL::ApplyBackpressure() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");
  // Create the fence for the current frame before waiting on the previous
  // frame's fence (to maximize CPU and GPU execution overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  if (current_context) {
    uint64_t this_frame_fence = current_context->BackpressureFenceCreate();
    current_context->BackpressureFenceWait(previous_frame_fence_);
    previous_frame_fence_ = this_frame_fence;
  }
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

  constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
  constexpr int kHistogramTimeBuckets = 50;

  // Query the underlying Metal device, if one exists. This is needed to ensure
  // synchronization between the display compositor and the HDRCopierLayer.
  // https://crbug.com/1372898
  if (gl::GLDisplayEGL* display =
          gl::GLDisplayEGL::GetDisplayForCurrentContext()) {
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

  // Do a GL fence for flush to apply back-pressure before drawing.
  {
    base::TimeTicks start_time = base::TimeTicks::Now();
    ApplyBackpressure();

    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Gpu.Mac.BackpressureUs", base::TimeTicks::Now() - start_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Update the CALayer tree in the GPU process.
  base::TimeTicks before_transaction_time = base::TimeTicks::Now();
  {
    TRACE_EVENT0("gpu", "CommitPendingTreesToCA");
    ca_layer_tree_coordinator_->CommitPendingTreesToCA();

    base::TimeDelta transaction_time =
        base::TimeTicks::Now() - before_transaction_time;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.IOSurface.CATransactionTimeUs", transaction_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Populate the CA layer parameters to send to the browser.
  gfx::CALayerParams params;
  {
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (use_remote_layer_api_) {
      params.ca_context_id = [ca_context_ contextId];
    } else {
      IOSurfaceRef io_surface =
          ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
      if (io_surface) {
        params.io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.pixel_size = pixel_size_;
    params.scale_factor = scale_factor_;
    params.is_empty = false;
  }

  // Send the swap parameters to the browser.
  if (completion_callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback),
                       gfx::SwapCompletionResult(
                           gfx::SwapResult::SWAP_ACK,
                           std::make_unique<gfx::CALayerParams>(params))));
  }
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::Hertz(60),
                                     /*flags=*/0);
  feedback.ca_layer_error_code = ca_layer_error_code_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageTransportSurfaceOverlayMacEGL::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), feedback));
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
  // TODO(1290313): the display_bounds might not need to be rounded to the
  // nearest rect as this eventually gets made into a CALayer. CALayers work in
  // floats.
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
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
  return true;
}

void ImageTransportSurfaceOverlayMacEGL::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

#if BUILDFLAG(IS_MAC)
bool ImageTransportSurfaceOverlayMacEGL::SupportsGpuVSync() const {
  return features::UseGpuVsync();
}

void ImageTransportSurfaceOverlayMacEGL::SetVSyncDisplayID(int64_t display_id) {
  if (!features::UseGpuVsync()) {
    return;
  }

  gpu_vsync_mac_->SetVSyncDisplayID(display_id);
}

void ImageTransportSurfaceOverlayMacEGL::SetGpuVSyncEnabled(bool enabled) {
  if (!features::UseGpuVsync()) {
    return;
  }

  gpu_vsync_mac_->SetGpuVSyncEnabled(enabled);
}
#endif

}  // namespace gpu
