// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_drawing_buffer_swap_chain.h"

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

namespace blink {

namespace {

bool FormatNeedsAlpha(GLenum layer_format) {
  switch (layer_format) {
    case GL_RGBA:
    case GL_RGBA8:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return true;

    case GL_RGB:
    case GL_RGB8:
    case GL_SRGB_EXT:
    case GL_SRGB8:
      return false;

    default:
      NOTREACHED();
  }
}

}  // namespace

XRWebGLDrawingBufferSwapChain::XRWebGLDrawingBufferSwapChain(
    WebGLRenderingContextBase* context,
    const XRWebGLSwapChain::Descriptor& descriptor,
    bool webgl2)
    : XRWebGLSwapChain(context, descriptor, webgl2) {
  CHECK(context);

  const bool want_antialiasing = false;
  const bool want_depth_buffer = false;
  const bool want_stencil_buffer = false;
  const bool want_alpha_channel = FormatNeedsAlpha(descriptor.format);
  gfx::Size desired_size(descriptor.width, descriptor.height);

  drawing_buffer_ = XRWebGLDrawingBuffer::Create(
      context->GetDrawingBuffer(), GetFramebuffer()->Object(), desired_size,
      want_alpha_channel, want_depth_buffer, want_stencil_buffer,
      want_antialiasing);
}

XRWebGLDrawingBufferSwapChain::~XRWebGLDrawingBufferSwapChain() {
  drawing_buffer_->BeginDestruction();
}

WebGLUnownedTexture* XRWebGLDrawingBufferSwapChain::ProduceTexture() {
  GLuint owned_texture = drawing_buffer_->GetCurrentColorBufferTextureId();

  return MakeGarbageCollected<WebGLUnownedTexture>(context(), owned_texture,
                                                   GL_TEXTURE_2D);
}

scoped_refptr<StaticBitmapImage>
XRWebGLDrawingBufferSwapChain::TransferToStaticBitmapImage() {
  WebGLUnownedTexture* texture = ResetCurrentTexture();
  if (texture) {
    // Notify our WebGLUnownedTexture that we have deleted it.
    static_cast<WebGLUnownedTexture*>(texture)->OnGLDeleteTextures();
  }
  return drawing_buffer_->TransferToStaticBitmapImage();
}

void XRWebGLDrawingBufferSwapChain::OnFrameEnd() {
  // ResetCurrentTexture handled in TransferToStaticBitmapImage.
}

void XRWebGLDrawingBufferSwapChain::Trace(Visitor* visitor) const {
  XRWebGLSwapChain::Trace(visitor);
}

}  // namespace blink
