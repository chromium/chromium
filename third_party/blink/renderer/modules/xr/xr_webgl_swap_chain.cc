// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_swap_chain.h"

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"

namespace blink {

XRWebGLSwapChain::XRWebGLSwapChain(
    WebGLRenderingContextBase* context,
    const XRWebGLSwapChain::Descriptor& descriptor,
    bool webgl2)
    : webgl_context_(context), descriptor_(descriptor), webgl2_(webgl2) {
  CHECK(context);
}

// Clears the contents of the current texture to transparent black or 0 (for
// depth/stencil textures).
void XRWebGLSwapChain::ClearCurrentTexture() {
  WebGLUnownedTexture* texture = current_texture();
  if (!texture) {
    return;
  }

  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return;
  }

  GLenum attachment = descriptor_.attachment_target;
  gl->BindFramebuffer(GL_FRAMEBUFFER, GetFramebuffer()->Object());

  GLbitfield clear_bits = 0;
  if (attachment == GL_COLOR_ATTACHMENT0) {
    clear_bits |= GL_COLOR_BUFFER_BIT;
    gl->ColorMask(true, true, true, true);
    gl->ClearColor(0, 0, 0, 0);
  } else if (attachment == GL_DEPTH_ATTACHMENT) {
    clear_bits |= GL_DEPTH_BUFFER_BIT;
    gl->DepthMask(true);
    gl->ClearDepthf(1.0f);
  } else if (attachment == GL_STENCIL_ATTACHMENT) {
    clear_bits |= GL_STENCIL_BUFFER_BIT;
    gl->StencilMaskSeparate(GL_FRONT, true);
    gl->ClearStencil(0);
  }

  gl->Disable(GL_SCISSOR_TEST);

  if (descriptor_.layers > 1) {
    for (uint32_t i = 0; i < descriptor_.layers; ++i) {
      gl->FramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture->Object(),
                                  0, i);
      gl->Clear(clear_bits);
    }
  } else if (IsCube()) {
    for (uint32_t i = 0; i < 6; ++i) {
      gl->FramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                               texture->Object(), 0);
      gl->Clear(clear_bits);
    }
  } else {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
                             texture->Object(), 0);
    gl->Clear(clear_bits);
  }

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());

  client->DrawingBufferClientRestoreScissorTest();
  client->DrawingBufferClientRestoreMaskAndClearValues();
  client->DrawingBufferClientRestoreFramebufferBinding();
}

WebGLFramebuffer* XRWebGLSwapChain::GetFramebuffer() {
  if (!framebuffer_) {
    framebuffer_ = webgl_context_->createFramebuffer();
  }
  return framebuffer_;
}

void XRWebGLSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
  XRSwapChain::Trace(visitor);
}

XRWebGLStaticSwapChain::XRWebGLStaticSwapChain(
    WebGLRenderingContextBase* context,
    const XRWebGLSwapChain::Descriptor& descriptor,
    bool webgl2)
    : XRWebGLSwapChain(context, descriptor, webgl2) {}

XRWebGLStaticSwapChain::~XRWebGLStaticSwapChain() {
  if (owned_texture_) {
    gpu::gles2::GLES2Interface* gl = context()->ContextGL();
    if (!gl) {
      return;
    }

    gl->DeleteTextures(1, &owned_texture_);
  }
}

WebGLUnownedTexture* XRWebGLStaticSwapChain::ProduceTexture() {
  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return nullptr;
  }

  GLenum target = descriptor().layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
  gl->GenTextures(1, &owned_texture_);
  gl->BindTexture(target, owned_texture_);

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());

  if (target == GL_TEXTURE_2D_ARRAY) {
    CHECK(webgl2());  // Texture arrays are only available in WebGL 2
    gl->TexStorage3D(target, 1, descriptor().internal_format,
                     descriptor().width, descriptor().height,
                     descriptor().layers);

    client->DrawingBufferClientRestoreTexture2DArrayBinding();
  } else {
    if (webgl2()) {
      gl->TexStorage2DEXT(target, 1, descriptor().internal_format,
                          descriptor().width, descriptor().height);
    } else {
      gl->TexImage2D(target, 0, descriptor().format, descriptor().width,
                     descriptor().height, 0, descriptor().format,
                     descriptor().type, nullptr);
    }

    client->DrawingBufferClientRestoreTexture2DBinding();
  }

  return MakeGarbageCollected<WebGLUnownedTexture>(context(), owned_texture_,
                                                   target);
}

void XRWebGLStaticSwapChain::OnFrameEnd() {
  ClearCurrentTexture();

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

XRWebGLSharedImageSwapChain::XRWebGLSharedImageSwapChain(
    WebGLRenderingContextBase* context,
    const XRWebGLSwapChain::Descriptor& descriptor,
    bool webgl2)
    : XRWebGLSwapChain(context, descriptor, webgl2) {
  // SharedImages cannot have multiple layers yet.
  CHECK_EQ(descriptor.layers, 1);
}

WebGLUnownedTexture* XRWebGLSharedImageSwapChain::ProduceTexture() {
  gpu::gles2::GLES2Interface* context_gl = context()->ContextGL();
  if (!context_gl) {
    return nullptr;
  }

  const XRSharedImageData& content_image_data = layer()->SharedImage();

  if (!content_image_data.shared_image) {
    return nullptr;
  }

  CHECK(content_image_data.sync_token.HasData());

  // Create a texture backed by the shared image.
  CHECK(!shared_image_texture_);
  shared_image_texture_ =
      content_image_data.shared_image->CreateGLTexture(context_gl);
  shared_image_scoped_access_ =
      shared_image_texture_->BeginAccess(content_image_data.sync_token,
                                         /*readonly=*/false);

  return MakeGarbageCollected<WebGLUnownedTexture>(
      context(), shared_image_texture_->id(), GL_TEXTURE_2D);
}

void XRWebGLSharedImageSwapChain::OnFrameEnd() {
  WebGLUnownedTexture* texture = ResetCurrentTexture();
  if (texture) {
    DCHECK(shared_image_texture_);
    gpu::SharedImageTexture::ScopedAccess::EndAccess(
        std::move(shared_image_scoped_access_));
    shared_image_texture_.reset();

    // Notify our WebGLUnownedTexture that we have deleted it.
    static_cast<WebGLUnownedTexture*>(texture)->OnGLDeleteTextures();
  }
}

}  // namespace blink
