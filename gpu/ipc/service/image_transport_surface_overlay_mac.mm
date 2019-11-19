// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_cgl.h"

namespace gpu {

template <typename BaseClass>
ImageTransportSurfaceOverlayMacBase<BaseClass>::
    ImageTransportSurfaceOverlayMacBase(
        base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : delegate_(delegate),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0),
      weak_ptr_factory_(this) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  static bool av_disabled_at_command_line =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

  ca_layer_tree_coordinator_.reset(new ui::CALayerTreeCoordinator(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer));
}

template <typename BaseClass>
ImageTransportSurfaceOverlayMacBase<
    BaseClass>::~ImageTransportSurfaceOverlayMacBase() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::Initialize(
    gl::GLSurfaceFormat format) {
  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  if (use_remote_layer_api_) {
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_.reset([
        [CAContext contextWithCGSConnection:connection_id options:@{}] retain]);
    [ca_context_ setLayer:ca_layer_tree_coordinator_->GetCALayerForDisplay()];
  }
  return true;
}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::PrepareToDestroy(
    bool have_context) {}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::Destroy() {
  ca_layer_tree_coordinator_.reset();
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::IsOffscreen() {
  return false;
}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::ApplyBackpressure() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");
  // Create the fence for the current frame before waiting on the previous
  // frame's fence (to maximize CPU and GPU execution overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  uint64_t this_frame_fence = current_context->BackpressureFenceCreate();
  current_context->BackpressureFenceWait(previous_frame_fence_);
  previous_frame_fence_ = this_frame_fence;
}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::BufferPresented(
    gl::GLSurface::PresentationCallback callback,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(feedback);
  if (delegate_)
    delegate_->BufferPresented(feedback);
}

template <typename BaseClass>
gfx::SwapResult
ImageTransportSurfaceOverlayMacBase<BaseClass>::SwapBuffersInternal(
    const gfx::Rect& pixel_damage_rect,
    gl::GLSurface::PresentationCallback callback) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  // Do a GL fence for flush to apply back-pressure before drawing.
  {
    SCOPED_UMA_HISTOGRAM_TIMER("Gpu.Mac.Backpressure");
    ApplyBackpressure();
  }

  // Update the CALayer tree in the GPU process.
  base::TimeTicks before_transaction_time = base::TimeTicks::Now();
  {
    TRACE_EVENT0("gpu", "CommitPendingTreesToCA");
    ca_layer_tree_coordinator_->CommitPendingTreesToCA(pixel_damage_rect);
    base::TimeTicks after_transaction_time = base::TimeTicks::Now();
    UMA_HISTOGRAM_TIMES("GPU.IOSurface.CATransactionTime",
                        after_transaction_time - before_transaction_time);
  }

  // Populate the swap-complete parameters to send to the browser.
  SwapBuffersCompleteParams params;
  {
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (use_remote_layer_api_) {
      params.ca_layer_params.ca_context_id = [ca_context_ contextId];
    } else {
      IOSurfaceRef io_surface =
          ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
      if (io_surface) {
        params.ca_layer_params.io_surface_mach_port.reset(
            IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.ca_layer_params.pixel_size = pixel_size_;
    params.ca_layer_params.scale_factor = scale_factor_;
    params.ca_layer_params.is_empty = false;
    params.swap_response.swap_id = 0;  // Set later, in DecoderClient.
    params.swap_response.result = gfx::SwapResult::SWAP_ACK;
    // TODO(brianderson): Tie swap_start to before_flush_time.
    params.swap_response.timings.swap_start = before_transaction_time;
    params.swap_response.timings.swap_end = before_transaction_time;
    for (auto& query : ca_layer_in_use_queries_) {
      gpu::TextureInUseResponse response;
      response.texture = query.texture;
      bool in_use = false;
      gl::GLImageIOSurface* io_surface_image =
          gl::GLImageIOSurface::FromGLImage(query.image.get());
      if (io_surface_image) {
        in_use = io_surface_image->CanCheckIOSurfaceIsInUse() &&
                 IOSurfaceIsInUse(io_surface_image->io_surface());
      }
      response.in_use = in_use;
      params.texture_in_use_responses.push_back(std::move(response));
    }
    ca_layer_in_use_queries_.clear();
  }

  // Send the swap parameters to the browser.
  delegate_->DidSwapBuffersComplete(std::move(params));
  constexpr int64_t kRefreshIntervalInMicroseconds =
      base::Time::kMicrosecondsPerSecond / 60;
  gfx::PresentationFeedback feedback(
      base::TimeTicks::Now(),
      base::TimeDelta::FromMicroseconds(kRefreshIntervalInMicroseconds),
      0 /* flags */);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ImageTransportSurfaceOverlayMacBase<BaseClass>::BufferPresented,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), feedback));
  return gfx::SwapResult::SWAP_ACK;
}

template <typename BaseClass>
gfx::SwapResult ImageTransportSurfaceOverlayMacBase<BaseClass>::SwapBuffers(
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(
      gfx::Rect(0, 0, pixel_size_.width(), pixel_size_.height()),
      std::move(callback));
}

template <typename BaseClass>
gfx::SwapResult ImageTransportSurfaceOverlayMacBase<BaseClass>::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(gfx::Rect(x, y, width, height),
                             std::move(callback));
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::SupportsPostSubBuffer() {
  return true;
}

template <typename BaseClass>
gfx::Size ImageTransportSurfaceOverlayMacBase<BaseClass>::GetSize() {
  return gfx::Size();
}

template <typename BaseClass>
void* ImageTransportSurfaceOverlayMacBase<BaseClass>::GetHandle() {
  return nullptr;
}

template <typename BaseClass>
gl::GLSurfaceFormat
ImageTransportSurfaceOverlayMacBase<BaseClass>::GetFormat() {
  return gl::GLSurfaceFormat();
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::OnMakeCurrent(
    gl::GLContext* context) {
  // Ensure that the context is on the appropriate GL renderer. The GL renderer
  // will generally only change when the GPU changes.
  if (gl_renderer_id_ && context)
    context->share_group()->SetRendererID(gl_renderer_id_);
  return true;
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gl::GLImage* image,
    const gfx::Rect& pixel_frame_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  if (transform != gfx::OVERLAY_TRANSFORM_NONE) {
    DLOG(ERROR) << "Invalid overlay plane transform.";
    return false;
  }
  if (z_order) {
    DLOG(ERROR) << "Invalid non-zero Z order.";
    return false;
  }
  gl::GLImageIOSurface* io_surface_image =
      gl::GLImageIOSurface::FromGLImage(image);
  if (!io_surface_image) {
    DLOG(ERROR) << "Not an IOSurface image.";
    return false;
  }
  const ui::CARendererLayerParams overlay_as_calayer_params(
      false,          // is_clipped
      gfx::Rect(),    // clip_rect
      gfx::RRectF(),  // rounded_corner_bounds
      0,              // sorting_context_id
      gfx::Transform(), image,
      crop_rect,            // contents_rect
      pixel_frame_rect,     // rect
      SK_ColorTRANSPARENT,  // background_color
      0,                    // edge_aa_mask
      1.f,                  // opacity
      GL_LINEAR);           // filter;
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(overlay_as_calayer_params);
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::ScheduleCALayer(
    const ui::CARendererLayerParams& params) {
  if (params.image) {
    gl::GLImageIOSurface* io_surface_image =
        gl::GLImageIOSurface::FromGLImage(params.image);
    if (!io_surface_image) {
      DLOG(ERROR) << "Cannot schedule CALayer with non-IOSurface GLImage";
      return false;
    }
  }
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(params);
}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::ScheduleCALayerInUseQuery(
    std::vector<gl::GLSurface::CALayerInUseQuery> queries) {
  ca_layer_in_use_queries_.swap(queries);
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::IsSurfaceless() const {
  return true;
}

template <typename BaseClass>
bool ImageTransportSurfaceOverlayMacBase<BaseClass>::Resize(
    const gfx::Size& pixel_size,
    float scale_factor,
    gl::GLSurface::ColorSpace color_space,
    bool has_alpha) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
  return true;
}

template <typename BaseClass>
void ImageTransportSurfaceOverlayMacBase<BaseClass>::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
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

  // Post a task holding a reference to the new GL context. The reason for
  // this is to avoid creating-then-destroying the context for every image
  // transport surface that is observing the GPU switch.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::DoNothing::Once<scoped_refptr<ui::IOSurfaceContext>>(),
          context_on_new_gpu));
}

// Template instantiation
template class ImageTransportSurfaceOverlayMacBase<gl::GLSurface>;
#if defined(USE_EGL)
template class ImageTransportSurfaceOverlayMacBase<gl::GLSurfaceEGL>;
#endif

}  // namespace gpu
