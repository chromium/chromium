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
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gpu_switching_manager.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_cgl.h"
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
    gl::GLDisplayEGL* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : gl::Presenter(display, gfx::Size()),
      delegate_(delegate),
#if BUILDFLAG(IS_MAC)
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
#endif
      scale_factor_(1),
      vsync_callback_(delegate->GetGpuVSyncCallback()),
      gl_renderer_id_(0),
      weak_ptr_factory_(this) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  static bool av_disabled_at_command_line =
      !base::FeatureList::IsEnabled(kAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

#if BUILDFLAG(IS_MAC)
  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer);

  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  if (use_remote_layer_api_) {
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_.reset([[CAContext contextWithCGSConnection:connection_id
                                                   options:@{}] retain]);
    [ca_context_ setLayer:ca_layer_tree_coordinator_->GetCALayerForDisplay()];
  }
#else
  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      /*allow_remote_layers=*/false, allow_av_sample_buffer_display_layer);
#endif
}

ImageTransportSurfaceOverlayMacEGL::~ImageTransportSurfaceOverlayMacEGL() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

bool ImageTransportSurfaceOverlayMacEGL::Initialize(
    gl::GLSurfaceFormat format) {
  return true;
}

void ImageTransportSurfaceOverlayMacEGL::PrepareToDestroy(bool have_context) {}

void ImageTransportSurfaceOverlayMacEGL::Destroy() {
  ca_layer_tree_coordinator_.reset();
}

void ImageTransportSurfaceOverlayMacEGL::ApplyBackpressure() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");
  // Create the fence for the current frame before waiting on the previous
  // frame's fence (to maximize CPU and GPU execution overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  uint64_t this_frame_fence = current_context->BackpressureFenceCreate();
  current_context->BackpressureFenceWait(previous_frame_fence_);
  previous_frame_fence_ = this_frame_fence;
}

void ImageTransportSurfaceOverlayMacEGL::BufferPresented(
    gl::GLSurface::PresentationCallback callback,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(feedback);
}

void ImageTransportSurfaceOverlayMacEGL::Present(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback,
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
      EGLAttrib metal_device = 0;
      if (eglQueryDeviceAttribEXT(angle_device, EGL_METAL_DEVICE_ANGLE,
                                  &metal_device)) {
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
#if BUILDFLAG(IS_MAC)
    if (use_remote_layer_api_) {
      params.ca_context_id = [ca_context_ contextId];
    } else {
#else
    if (true) {
#endif
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

bool ImageTransportSurfaceOverlayMacEGL::SupportsCommitOverlayPlanes() {
  return true;
}

gfx::Size ImageTransportSurfaceOverlayMacEGL::GetSize() {
  return gfx::Size();
}

void* ImageTransportSurfaceOverlayMacEGL::GetHandle() {
  return nullptr;
}

gl::GLSurfaceFormat ImageTransportSurfaceOverlayMacEGL::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool ImageTransportSurfaceOverlayMacEGL::OnMakeCurrent(gl::GLContext* context) {
  // Ensure that the context is on the appropriate GL renderer. The GL renderer
  // will generally only change when the GPU changes.
  if (gl_renderer_id_ && context)
    context->share_group()->SetRendererID(gl_renderer_id_);
  return true;
}

bool ImageTransportSurfaceOverlayMacEGL::ScheduleOverlayPlane(
    gl::OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (overlay_plane_data.plane_transform != gfx::OVERLAY_TRANSFORM_NONE) {
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
      false,          // is_clipped
      gfx::Rect(),    // clip_rect
      gfx::RRectF(),  // rounded_corner_bounds
      0,              // sorting_context_id
      gfx::Transform(), image, overlay_plane_data.color_space,
      overlay_plane_data.crop_rect,                           // contents_rect
      gfx::ToNearestRect(overlay_plane_data.display_bounds),  // rect
      SkColors::kTransparent,  // background_color
      0,                       // edge_aa_mask
      1.f,                     // opacity
      GL_LINEAR,               // filter
      gfx::HDRMode::kDefault,
      absl::nullopt,                     // hdr_metadata
      gfx::ProtectedVideoType::kClear);  // protected_video_type
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

void ImageTransportSurfaceOverlayMacEGL::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
#if BUILDFLAG(IS_MAC)
  // Create a new context, and use the GL renderer ID that the new context gets.
  scoped_refptr<ui::IOSurfaceContext> context_on_new_gpu =
      ui::IOSurfaceContext::Get(ui::IOSurfaceContext::kCALayerContext);
  if (!context_on_new_gpu)
    return;
  GLint context_renderer_id = -1;
  if (CGLGetParameter(context_on_new_gpu->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &context_renderer_id) != kCGLNoError) {
    LOG(ERROR) << "Failed to create test context after GPU switch";
    return;
  }
  gl_renderer_id_ = context_renderer_id & kCGLRendererIDMatchingMask;

  // Delay releasing the reference to the new GL context. The reason for this
  // is to avoid creating-then-destroying the context for every image transport
  // surface that is observing the GPU switch.
  base::SingleThreadTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(context_on_new_gpu));
#endif
}

void ImageTransportSurfaceOverlayMacEGL::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

void ImageTransportSurfaceOverlayMacEGL::SetVSyncDisplayID(int64_t display_id) {
}

bool ImageTransportSurfaceOverlayMacEGL::SupportsGpuVSync() const {
  return features::UseGpuVsync();
}

void ImageTransportSurfaceOverlayMacEGL::SetGpuVSyncEnabled(bool enabled) {
  if (gpu_vsync_enabled_ == enabled) {
    return;
  }

  gpu_vsync_enabled_ = enabled;
}

}  // namespace gpu
