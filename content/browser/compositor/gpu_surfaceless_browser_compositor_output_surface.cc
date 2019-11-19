// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

#include <utility>

#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "content/browser/compositor/reflector_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gl/gl_enums.h"

namespace content {

GpuSurfacelessBrowserCompositorOutputSurface::
    GpuSurfacelessBrowserCompositorOutputSurface(
        scoped_refptr<viz::ContextProviderCommandBuffer> context,
        gpu::SurfaceHandle surface_handle,
        gfx::BufferFormat format,
        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuBrowserCompositorOutputSurface(std::move(context), surface_handle),
      use_gpu_fence_(
          context_provider_->ContextCapabilities().chromium_gpu_fence &&
          context_provider_->ContextCapabilities()
              .use_gpu_fences_for_overlay_planes),
      gpu_fence_id_(0),
      current_texture_(0u),
      fbo_(0u),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  auto* gl = context_provider_->ContextGL();
  buffer_queue_.reset(new viz::BufferQueue(
      gl, format, gpu_memory_buffer_manager_, surface_handle,
      context_provider_->ContextCapabilities()));
  gl->GenFramebuffers(1, &fbo_);
}

GpuSurfacelessBrowserCompositorOutputSurface::
    ~GpuSurfacelessBrowserCompositorOutputSurface() {
  auto* gl = context_provider_->ContextGL();
  if (gpu_fence_id_ > 0)
    gl->DestroyGpuFenceCHROMIUM(gpu_fence_id_);
  DCHECK_NE(0u, fbo_);
  gl->DeleteFramebuffers(1, &fbo_);
}

bool GpuSurfacelessBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane()
    const {
  return true;
}

unsigned GpuSurfacelessBrowserCompositorOutputSurface::GetOverlayTextureId()
    const {
  DCHECK(current_texture_);
  return current_texture_;
}

gfx::BufferFormat
GpuSurfacelessBrowserCompositorOutputSurface::GetOverlayBufferFormat() const {
  DCHECK(buffer_queue_);
  return buffer_queue_->buffer_format();
}

void GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(
    viz::OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);
  DCHECK(reshape_size_ == frame.size);
  // TODO(ccameron): What if a swap comes again before OnGpuSwapBuffersCompleted
  // happens, we'd see the wrong swap size there?
  swap_size_ = reshape_size_;

  gfx::Rect damage_rect =
      frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(swap_size_);
  buffer_queue_->SwapBuffers(damage_rect);

  GpuBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
}

void GpuSurfacelessBrowserCompositorOutputSurface::BindFramebuffer() {
  auto* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  DCHECK(buffer_queue_);
  unsigned stencil;
  current_texture_ = buffer_queue_->GetCurrentBuffer(&stencil);
  if (!current_texture_)
    return;
  // TODO(andrescj): if the texture hasn't changed since the last call to
  // BindFrameBuffer(), we may be able to avoid mutating the FBO which may lead
  // to performance improvements.
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           buffer_queue_->texture_target(), current_texture_,
                           0);

#if DCHECK_IS_ON() && defined(OS_CHROMEOS)
  const GLenum result = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE)
    DLOG(ERROR) << " Incomplete fb: " << gl::GLEnums::GetStringError(result);
#endif

  if (stencil) {
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, stencil);
  }
}

gfx::Rect
GpuSurfacelessBrowserCompositorOutputSurface::GetCurrentFramebufferDamage()
    const {
  return buffer_queue_->CurrentBufferDamage();
}

GLenum GpuSurfacelessBrowserCompositorOutputSurface::
    GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

void GpuSurfacelessBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  reshape_size_ = size;
  GpuBrowserCompositorOutputSurface::Reshape(
      size, device_scale_factor, color_space, has_alpha, use_stencil);
  DCHECK(buffer_queue_);
  if (buffer_queue_->Reshape(size, device_scale_factor, color_space,
                             use_stencil)) {
    auto* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             buffer_queue_->texture_target(), 0, 0);
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
  }
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    const gpu::SwapBuffersCompleteParams& params) {
  gpu::SwapBuffersCompleteParams modified_params(params);
  bool force_swap = false;
  if (params.swap_response.result ==
      gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    modified_params.swap_response.result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->FreeAllSurfaces();
    force_swap = true;
  }
  buffer_queue_->PageFlipComplete();
  GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
      std::move(latency_info), modified_params);
  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

unsigned GpuSurfacelessBrowserCompositorOutputSurface::UpdateGpuFence() {
  if (!use_gpu_fence_)
    return 0;

  auto* gl = context_provider_->ContextGL();
  if (gpu_fence_id_ > 0)
    gl->DestroyGpuFenceCHROMIUM(gpu_fence_id_);

  gpu_fence_id_ = gl->CreateGpuFenceCHROMIUM();

  return gpu_fence_id_;
}

void GpuSurfacelessBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& damage) {
  GpuBrowserCompositorOutputSurface::SetDrawRectangle(damage);
}

}  // namespace content
