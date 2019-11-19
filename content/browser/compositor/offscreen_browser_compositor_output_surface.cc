// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using gpu::gles2::GLES2Interface;

namespace content {

static viz::ResourceFormat kFboTextureFormat = viz::RGBA_8888;

OffscreenBrowserCompositorOutputSurface::
    OffscreenBrowserCompositorOutputSurface(
        scoped_refptr<viz::ContextProviderCommandBuffer> context)
    : BrowserCompositorOutputSurface(std::move(context)) {
  capabilities_.uses_default_gl_framebuffer = false;
}

OffscreenBrowserCompositorOutputSurface::
    ~OffscreenBrowserCompositorOutputSurface() {
  DiscardBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::BindToClient(
    viz::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void OffscreenBrowserCompositorOutputSurface::EnsureBackbuffer() {
  bool update_source_texture = !reflector_texture_ || reflector_changed_;
  reflector_changed_ = false;
  if (!reflector_texture_) {
    reflector_texture_.reset(new ReflectorTexture(context_provider()));

    GLES2Interface* gl = context_provider_->ContextGL();

    const int max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
    int texture_width = std::min(max_texture_size, reshape_size_.width());
    int texture_height = std::min(max_texture_size, reshape_size_.height());

    gl->BindTexture(GL_TEXTURE_2D, reflector_texture_->texture_id());
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(kFboTextureFormat),
                   texture_width, texture_height, 0,
                   GLDataFormat(kFboTextureFormat),
                   GLDataType(kFboTextureFormat), nullptr);
    if (!fbo_)
      gl->GenFramebuffers(1, &fbo_);

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, reflector_texture_->texture_id(),
                             0);
  }

  // The reflector may be created later or detached and re-attached,
  // so don't assume it always exists. For example, ChromeOS always
  // creates a reflector asynchronosly when creating this for software
  // mirroring.  See |DisplayManager::CreateMirrorWindowAsyncIfAny|.
  if (reflector_ && update_source_texture)
    reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
}

void OffscreenBrowserCompositorOutputSurface::DiscardBackbuffer() {
  GLES2Interface* gl = context_provider_->ContextGL();

  if (reflector_texture_) {
    reflector_texture_.reset();
    if (reflector_)
      reflector_->OnSourceTextureMailboxUpdated(nullptr);
  }

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
}

void OffscreenBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {}

void OffscreenBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool alpha,
    bool stencil) {
  reshape_size_ = size;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::BindFramebuffer() {
  bool need_to_bind = !!reflector_texture_.get();

  EnsureBackbuffer();
  DCHECK(reflector_texture_.get());
  DCHECK(fbo_);

  if (need_to_bind) {
    GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }
}

void OffscreenBrowserCompositorOutputSurface::SwapBuffers(
    viz::OutputSurfaceFrame frame) {
  gfx::Size surface_size = frame.size;
  DCHECK(surface_size == reshape_size_);

  if (reflector_) {
    if (frame.sub_buffer_rect)
      reflector_->OnSourcePostSubBuffer(*frame.sub_buffer_rect, surface_size);
    else
      reflector_->OnSourceSwapBuffers(surface_size);
  }

  // TODO(oshima): sync with the reflector's SwapBuffersComplete
  // (crbug.com/520567).
  // The original implementation had a flickering issue (crbug.com/515332).
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token,
      base::BindOnce(
          &OffscreenBrowserCompositorOutputSurface::OnSwapBuffersComplete,
          weak_ptr_factory_.GetWeakPtr(), frame.latency_info));
}

bool OffscreenBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane()
    const {
  return false;
}

unsigned OffscreenBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat
OffscreenBrowserCompositorOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

GLenum
OffscreenBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  return GLCopyTextureInternalFormat(kFboTextureFormat);
}

void OffscreenBrowserCompositorOutputSurface::OnReflectorChanged() {
  if (reflector_) {
    reflector_changed_ = true;
    EnsureBackbuffer();
  }
}

void OffscreenBrowserCompositorOutputSurface::OnSwapBuffersComplete(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);
  // Swap timings are not available since for offscreen there is no Swap, just
  // a SignalSyncToken.
  client_->DidReceiveSwapBuffersAck(gfx::SwapTimings());
  client_->DidReceivePresentationFeedback(gfx::PresentationFeedback());
}

unsigned OffscreenBrowserCompositorOutputSurface::UpdateGpuFence() {
  return 0;
}

}  // namespace content
