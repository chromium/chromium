// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_webgl_layer_init.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_client.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport_delegate.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"

namespace blink {

class WebGLFramebuffer;
class WebGLRenderingContextBase;
class XRSession;
class XRViewport;

class XRWebGLLayer final : public XRLayer, public XrLayerClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRWebGLLayer(XRSession*,
               WebGLRenderingContextBase*,
               scoped_refptr<XRWebGLDrawingBuffer>,
               WebGLFramebuffer*,
               double framebuffer_scale,
               bool ignore_depth_values);
  ~XRWebGLLayer() override;

  static XRWebGLLayer* Create(XRSession*,
                              const V8XRWebGLRenderingContext*,
                              const XRWebGLLayerInit*,
                              ExceptionState&);

  // XrLayerClient overrides.
  XRSession* session() const override;
  std::unique_ptr<SharedImageHolder> TransferToSharedImageHolder() override;
  XRFrameTransportDelegate* GetTransportDelegate() override;
  std::unique_ptr<SharedImageHolder> DoneWithSharedBuffer() override;

  WebGLFramebuffer* framebuffer() const { return framebuffer_.Get(); }
  uint32_t framebufferWidth() const;
  uint32_t framebufferHeight() const;

  bool antialias() const;
  bool ignoreDepthValues() const { return ignore_depth_values_; }

  XRViewport* getViewport(XRView*);

  static double getNativeFramebufferScaleFactor(XRSession* session);

  XRViewport* GetViewportForEye(device::mojom::blink::XREye);

  void UpdateViewports();

  HTMLCanvasElement* output_canvas() const;

  void OnFrameStart() override;
  void OnFrameEnd() override;
  void OnResize() override;

  XRLayerType LayerType() const override;

  XrLayerClient* LayerClient() override;

  WebGLRenderingContextBase* GetWebGLContext() { return webgl_context_; }

  void Trace(Visitor*) const override;

 protected:
  device::mojom::blink::XRCompositionLayerDataPtr CreateLayerData()
      const override;

 private:
  Member<XRViewport> left_viewport_;
  Member<XRViewport> right_viewport_;

  Member<WebGLRenderingContextBase> webgl_context_;
  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer_;
  Member<WebGLFramebuffer> framebuffer_;

  double framebuffer_scale_ = 1.0;
  bool viewports_dirty_ = true;
  bool is_direct_draw_frame = false;
  bool ignore_depth_values_ = false;

  uint32_t clean_frame_count = 0;

  Member<XRWebGLFrameTransportDelegate> transport_delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
