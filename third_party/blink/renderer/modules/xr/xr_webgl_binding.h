// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_BINDING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_BINDING_H_

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;
class V8XREye;
class WebGLRenderingContextBase;
class WebGLTexture;
class XRCamera;
class XRCompositionLayer;
class XRCubeLayer;
class XRCubeLayerInit;
class XRCylinderLayer;
class XRCylinderLayerInit;
class XREquirectLayer;
class XREquirectLayerInit;
class XRFrame;
class XRLayerInit;
class XRLightProbe;
class XRSession;
class XRView;
class XRWebGLDepthInformation;
class XRProjectionLayer;
class XRProjectionLayerInit;
class XRQuadLayer;
class XRQuadLayerInit;
class V8XRTextureType;
class XRWebGLSubImage;
class XRWebGLSwapChain;

class XRWebGLBinding final : public ScriptWrappable, public XRGraphicsBinding {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRWebGLBinding(XRSession*, WebGLRenderingContextBase*, bool webgl2);
  ~XRWebGLBinding() override = default;

  static XRWebGLBinding* Create(XRSession* session,
                                const V8XRWebGLRenderingContext* context,
                                ExceptionState& exception_state);

  bool usesDepthValues() const;

  XRProjectionLayer* createProjectionLayer(const XRProjectionLayerInit*,
                                           ExceptionState&);

  XRQuadLayer* createQuadLayer(const XRQuadLayerInit*, ExceptionState&);

  XREquirectLayer* createEquirectLayer(const XREquirectLayerInit*,
                                       ExceptionState&);

  XRCylinderLayer* createCylinderLayer(const XRCylinderLayerInit*,
                                       ExceptionState&);

  XRCubeLayer* createCubeLayer(const XRCubeLayerInit*, ExceptionState&);

  XRWebGLSubImage* getViewSubImage(XRProjectionLayer* layer,
                                   XRView* view,
                                   ExceptionState& exception_state);

  XRWebGLSubImage* getSubImage(XRCompositionLayer* layer,
                               XRFrame* frame,
                               V8XREye eye,
                               ExceptionState& exception_state);

  WebGLTexture* getReflectionCubeMap(XRLightProbe*, ExceptionState&);

  WebGLTexture* getCameraImage(XRCamera* camera,
                               ExceptionState& exception_state);

  XRWebGLDepthInformation* getDepthInformation(XRView* view,
                                               ExceptionState& exception_state);

  gfx::Rect GetViewportForView(XRProjectionLayer* layer,
                               XRViewData* view) override;

  WebGLRenderingContextBase* context() const { return webgl_context_.Get(); }

  XRFrameTransportDelegate* GetTransportDelegate() override;

  void Trace(Visitor*) const override;

 private:
  bool ValidateSessionAndContext(ExceptionState&);
  bool ValidateLayerColorFormat(GLenum color_format,
                                ExceptionState& exception_state);
  bool ValidateLayerDepthStencilFormat(GLenum depth_stencil_format,
                                       ExceptionState& exception_state);
  bool CanCreateShapedLayer(const XRLayerInit*, ExceptionState&);
  bool ValidateShapedLayerTextureType(const V8XRTextureType, ExceptionState&);
  bool ValidateShapedLayerData(const XRLayerInit*, ExceptionState&);
  GLenum FormatForLayerFormat(GLenum format);
  GLenum InternalFormatForLayerFormat(GLenum format);
  GLenum TypeForLayerFormat(GLenum format);

  gfx::Size GetTextureSizeForLayer(const XRLayerInit*) const;
  gfx::Rect GetViewportForLayer(const XRCompositionLayer&, V8XREye) const;

  XRWebGLSwapChain* CreateColorSwapchain(GLenum layer_format,
                                         gfx::Size layer_size);
  XRWebGLSwapChain* GetSwapchainForLayer(XRCompositionLayer* layer);

  Member<WebGLRenderingContextBase> webgl_context_;
  bool webgl2_;

  Member<XRWebGLFrameTransportDelegate> transport_delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_BINDING_H_
