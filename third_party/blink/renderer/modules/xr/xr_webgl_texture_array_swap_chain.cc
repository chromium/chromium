// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_texture_array_swap_chain.h"

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"

namespace blink {

namespace {

XRWebGLSwapChain::Descriptor MakeLayerDescriptor(
    XRWebGLSwapChain* wrapped_swap_chain,
    uint32_t layers) {
  // Copy the wrapped swap chain's descriptor and divide its width by the
  // number of requested layers.
  XRWebGLSwapChain::Descriptor descriptor = wrapped_swap_chain->descriptor();

  CHECK_EQ(descriptor.width % layers, 0ul);
  descriptor.width /= layers;
  descriptor.layers = layers;
  return descriptor;
}

}  // namespace

XRWebGLTextureArraySwapChain::XRWebGLTextureArraySwapChain(
    XRWebGLSwapChain* wrapped_swap_chain,
    uint32_t layers)
    : XRWebGLSwapChain(wrapped_swap_chain->context(),
                       MakeLayerDescriptor(wrapped_swap_chain, layers),
                       wrapped_swap_chain->webgl2()),
      wrapped_swap_chain_(wrapped_swap_chain) {
  CHECK(wrapped_swap_chain_);
  CHECK(webgl2());  // Texture arrays are only available in WebGL 2
}

XRWebGLTextureArraySwapChain::~XRWebGLTextureArraySwapChain() {
  if (owned_texture_) {
    gpu::gles2::GLES2Interface* gl = context()->ContextGL();
    if (!gl) {
      return;
    }

    gl->DeleteTextures(1, &owned_texture_);
  }
}

WebGLUnownedTexture* XRWebGLTextureArraySwapChain::ProduceTexture() {
  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return nullptr;
  }

  gl->GenTextures(1, &owned_texture_);
  gl->BindTexture(GL_TEXTURE_2D_ARRAY, owned_texture_);
  gl->TexStorage3D(GL_TEXTURE_2D_ARRAY, 1, descriptor().internal_format,
                   descriptor().width, descriptor().height,
                   descriptor().layers);

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());
  client->DrawingBufferClientRestoreTexture2DArrayBinding();

  return MakeGarbageCollected<WebGLUnownedTexture>(context(), owned_texture_,
                                                   GL_TEXTURE_2D_ARRAY);
}

void XRWebGLTextureArraySwapChain::SetLayer(XRCompositionLayer* layer) {
  XRWebGLSwapChain::SetLayer(layer);
  wrapped_swap_chain_->SetLayer(layer);
}

void XRWebGLTextureArraySwapChain::OnFrameStart() {
  wrapped_swap_chain_->OnFrameStart();
}

void XRWebGLTextureArraySwapChain::OnFrameEnd() {
  if (!texture_was_queried()) {
    wrapped_swap_chain_->OnFrameEnd();
    return;
  }

  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return;
  }

  // Copy from the layers texture to the side-by-side wrapped texture.
  WebGLUnownedTexture* source_texture = GetCurrentTexture();
  WebGLUnownedTexture* wrapped_texture =
      wrapped_swap_chain_->GetCurrentTexture();

  GLenum attachment = descriptor().attachment_target;
  gl->BindFramebuffer(GL_FRAMEBUFFER, GetFramebuffer()->Object());
  gl->BindTexture(GL_TEXTURE_2D, wrapped_texture->Object());

  // TODO(crbug.com/391919452): Driver bug is preventing copy from the correct
  // layer on some devices.
  for (uint32_t i = 0; i < descriptor().layers; ++i) {
    GLint x_offset = descriptor().width * i;
    gl->FramebufferTextureLayer(GL_FRAMEBUFFER, attachment,
                                source_texture->Object(), 0, i);
    gl->CopyTexSubImage2D(GL_TEXTURE_2D, 0, x_offset, 0, 0, 0,
                          descriptor().width, descriptor().height);
  }

  // ClearCurrentTexture resets the framebuffer binding prior to returning.
  ClearCurrentTexture();

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());
  client->DrawingBufferClientRestoreTexture2DBinding();

  wrapped_swap_chain_->OnFrameEnd();

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

void XRWebGLTextureArraySwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(wrapped_swap_chain_);
  XRWebGLSwapChain::Trace(visitor);
}

}  // namespace blink
