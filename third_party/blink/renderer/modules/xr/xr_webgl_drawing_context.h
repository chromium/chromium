// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_DRAWING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_DRAWING_CONTEXT_H_

#include "third_party/blink/renderer/modules/xr/xr_layer_drawing_context.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer_client.h"

namespace blink {

class WebGLRenderingContextBase;
class XRWebGLBinding;
class XRWebGLSwapChain;
class XRCompositionLayer;

class XRWebGLDrawingContext final : public XRLayerDrawingContext,
                                    public XRWebGLLayerClient {
 public:
  XRWebGLDrawingContext(XRWebGLBinding*,
                        XRWebGLSwapChain* color_swap_chain,
                        XRWebGLSwapChain* depth_stencil_swap_chain);
  ~XRWebGLDrawingContext() = default;

  enum XRGraphicsBinding::Api GraphicsApi() const override;

  // XRWebGLLayerClient implementation.
  const XRLayer* layer() const override;
  WebGLRenderingContextBase* context() const override {
    return webgl_context_.Get();
  }
  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage() override;

  uint16_t TextureWidth() const override;
  uint16_t TextureHeight() const override;
  uint16_t TextureArrayLength() const override;

  bool TextureWasQueried() const override;

  void SetCompositionLayer(XRCompositionLayer* layer) override;

  void OnFrameStart() override;
  void OnFrameEnd() override;

  XRWebGLSwapChain* color_swap_chain() const { return color_swap_chain_.Get(); }
  XRWebGLSwapChain* depth_stencil_swap_chain() const {
    return depth_stencil_swap_chain_.Get();
  }

  void Trace(Visitor*) const override;

 private:
  Member<WebGLRenderingContextBase> webgl_context_;
  Member<XRWebGLSwapChain> color_swap_chain_;
  Member<XRWebGLSwapChain> depth_stencil_swap_chain_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_DRAWING_CONTEXT_H_
