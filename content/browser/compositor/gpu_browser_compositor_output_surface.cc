// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gl/color_space_utils.h"

namespace content {

GpuBrowserCompositorOutputSurface::GpuBrowserCompositorOutputSurface(
    scoped_refptr<viz::ContextProviderCommandBuffer> context,
    gpu::SurfaceHandle surface_handle)
    : BrowserCompositorOutputSurface(std::move(context)),
      surface_handle_(surface_handle) {
  capabilities_.only_invalidates_damage_rect = false;
  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().flips_vertically;
  }
  capabilities_.supports_stencil =
      context_provider()->ContextCapabilities().num_stencil_bits > 0;
  // Since one of the buffers is used by the surface for presentation, there can
  // be at most |num_surface_buffers - 1| pending buffers that the compositor
  // can use.
  capabilities_.max_frames_pending =
      context_provider()->ContextCapabilities().num_surface_buffers - 1;
}

GpuBrowserCompositorOutputSurface::~GpuBrowserCompositorOutputSurface() =
    default;

void GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    const gpu::SwapBuffersCompleteParams& params) {
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  client_->DidReceiveSwapBuffersAck(params.swap_response.timings);
  UpdateLatencyInfoOnSwap(params.swap_response, &latency_info);
  latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);
}

void GpuBrowserCompositorOutputSurface::OnReflectorChanged() {
  if (!reflector_) {
    reflector_texture_.reset();
  } else {
    reflector_texture_.reset(new ReflectorTexture(context_provider()));
    reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
  }
  reflector_texture_defined_ = false;
}

void GpuBrowserCompositorOutputSurface::BindToClient(
    viz::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;

  GetCommandBufferProxy()->SetUpdateVSyncParametersCallback(base::BindRepeating(
      &GpuBrowserCompositorOutputSurface::OnUpdateVSyncParameters,
      weak_ptr_factory_.GetWeakPtr()));
}

void GpuBrowserCompositorOutputSurface::EnsureBackbuffer() {}

void GpuBrowserCompositorOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void GpuBrowserCompositorOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  size_ = size;
  has_set_draw_rectangle_since_last_resize_ = false;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor,
      gl::ColorSpaceUtils::GetGLColorSpace(color_space), has_alpha);
}

void GpuBrowserCompositorOutputSurface::SwapBuffers(
    viz::OutputSurfaceFrame frame) {
  gfx::Size surface_size = frame.size;
  if (reflector_) {
    if (frame.sub_buffer_rect && reflector_texture_defined_) {
      reflector_texture_->CopyTextureSubImage(*frame.sub_buffer_rect);
      reflector_->OnSourcePostSubBuffer(*frame.sub_buffer_rect, surface_size);
    } else {
      reflector_texture_->CopyTextureFullImage(surface_size);
      reflector_->OnSourceSwapBuffers(surface_size);
      reflector_texture_defined_ = true;
    }
  }

  set_draw_rectangle_for_frame_ = false;

  auto swap_callback = base::BindOnce(
      &GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(frame.latency_info));
  uint32_t flags = gpu::SwapBuffersFlags::kVSyncParams;
  gpu::ContextSupport::PresentationCallback presentation_callback;
  presentation_callback =
      base::BindOnce(&GpuBrowserCompositorOutputSurface::OnPresentation,
                     weak_ptr_factory_.GetWeakPtr());
  if (frame.sub_buffer_rect) {
    DCHECK(frame.content_bounds.empty());
    context_provider_->ContextSupport()->PartialSwapBuffers(
        *frame.sub_buffer_rect, flags, std::move(swap_callback),
        std::move(presentation_callback));
  } else if (!frame.content_bounds.empty()) {
    context_provider_->ContextSupport()->SwapWithBounds(
        frame.content_bounds, flags, std::move(swap_callback),
        std::move(presentation_callback));
  } else {
    context_provider_->ContextSupport()->Swap(flags, std::move(swap_callback),
                                              std::move(presentation_callback));
  }
}

uint32_t GpuBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl =
      static_cast<viz::ContextProviderCommandBuffer*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

bool GpuBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned GpuBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat GpuBrowserCompositorOutputSurface::GetOverlayBufferFormat()
    const {
  return gfx::BufferFormat::RGBX_8888;
}

void GpuBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& rect) {
  if (!context_provider_->ContextCapabilities().dc_layers)
    return;

  if (set_draw_rectangle_for_frame_)
    return;
  DCHECK(gfx::Rect(size_).Contains(rect));
  DCHECK(has_set_draw_rectangle_since_last_resize_ ||
         (gfx::Rect(size_) == rect));
  set_draw_rectangle_for_frame_ = true;
  has_set_draw_rectangle_since_last_resize_ = true;
  context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
      rect.x(), rect.y(), rect.width(), rect.height());
}

void GpuBrowserCompositorOutputSurface::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
}

void GpuBrowserCompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (update_vsync_parameters_callback_)
    update_vsync_parameters_callback_.Run(timebase, interval);
}

gpu::CommandBufferProxyImpl*
GpuBrowserCompositorOutputSurface::GetCommandBufferProxy() {
  viz::ContextProviderCommandBuffer* provider_command_buffer =
      static_cast<viz::ContextProviderCommandBuffer*>(context_provider_.get());
  gpu::CommandBufferProxyImpl* command_buffer_proxy =
      provider_command_buffer->GetCommandBufferProxy();
  DCHECK(command_buffer_proxy);
  return command_buffer_proxy;
}

unsigned GpuBrowserCompositorOutputSurface::UpdateGpuFence() {
  return 0;
}

void GpuBrowserCompositorOutputSurface::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

gfx::OverlayTransform GpuBrowserCompositorOutputSurface::GetDisplayTransform() {
  return display_transform_;
}

gpu::SurfaceHandle GpuBrowserCompositorOutputSurface::GetSurfaceHandle() const {
  return surface_handle_;
}

}  // namespace content
