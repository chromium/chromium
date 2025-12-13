// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_CUBEMAP_SWAP_CHAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_CUBEMAP_SWAP_CHAIN_H_

#include "third_party/blink/renderer/modules/xr/xr_webgl_swap_chain.h"

namespace blink {

// This swapchain is a shim which wraps another swapchain that has 6
// square sub-images laid out as 3 tiles per row, and produces a cubemap
// instead. When the frame ends the contents of each face of the cubemap are
// copied to the corresponding location in the texture 2d. This obviously incurs
// undesirable overhead, and it  would be ideal if we could use the cubemap
// swapchains directly, but not all drivers support cubemap buffers. See
// crbug.com/459811463.
class XRWebGLCubemapSwapChain final : public XRWebGLSwapChain {
 public:
  explicit XRWebGLCubemapSwapChain(XRWebGLSwapChain* wrapped_swapchain);
  ~XRWebGLCubemapSwapChain() override;

  bool IsCube() const override { return true; }
  WebGLUnownedTexture* ProduceTexture() override;

  void OnFrameStart() override;
  void OnFrameEnd() override;

  void SetLayer(XRCompositionLayer* layer) override;

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage() override;

  void Trace(Visitor* visitor) const override;

 private:
  Member<WebGLUnownedTexture> owned_texture_;
  Member<XRWebGLSwapChain> wrapped_swapchain_;
  GLuint vertex_buffer_ = 0;
  GLuint index_buffer_ = 0;
  GLuint copy_program_ = 0;
  GLint texture_uniform_ = 0;
  GLint face_index_uniform_ = 0;
  GLint position_handle_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_CUBEMAP_SWAP_CHAIN_H_
