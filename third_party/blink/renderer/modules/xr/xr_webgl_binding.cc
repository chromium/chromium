// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_binding.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"

namespace blink {

XRWebGLBinding* XRWebGLBinding::Create(XRSession* session,
                                       const XRWebGLRenderingContext& context,
                                       ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  if (!session->immersive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding for an "
                                      "inline XRSession.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context =
      webglRenderingContextBaseFromUnion(context);

  if (webgl_context->isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding with a "
                                      "lost WebGL context.");
    return nullptr;
  }

  if (!webgl_context->IsXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGL context must be marked as XR compatible in order to "
        "use with an immersive XRSession");
    return nullptr;
  }

  return MakeGarbageCollected<XRWebGLBinding>(
      session, webgl_context, context.IsWebGL2RenderingContext());
}

XRWebGLBinding::XRWebGLBinding(XRSession* session,
                               WebGLRenderingContextBase* webgl_context,
                               bool webgl2)
    : session_(session), webgl_context_(webgl_context), webgl2_(webgl2) {}

WebGLTexture* XRWebGLBinding::getReflectionCubeMap(
    XRLightProbe* light_probe,
    ExceptionState& exception_state) {
  if (!webgl2_ && !webgl_context_->ExtensionsUtil()->IsExtensionEnabled(
                      "OES_texture_half_float")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGL contexts must have the OES_texture_half_float extension enabled "
        "prior to calling getReflectionCubeMap. This restriction does not "
        "apply to WebGL 2.0 contexts.");
    return nullptr;
  }

  XRCubeMap* cube_map = light_probe->getReflectionCubeMap();
  if (!cube_map) {
    return nullptr;
  }

  WebGLTexture* texture = MakeGarbageCollected<WebGLTexture>(webgl_context_);
  cube_map->updateWebGLEnvironmentCube(webgl_context_, texture);

  return texture;
}

WebGLTexture* XRWebGLBinding::getCameraImage(XRFrame* frame, XRView* view) {
  // Verify that frame is currently active.
  if (!frame->IsActive()) {
    return nullptr;
  }

  if (frame != view->frame()) {
    return nullptr;
  }

  XRWebGLLayer* base_layer = view->session()->renderState()->baseLayer();
  DCHECK(base_layer);

  base::Optional<gpu::MailboxHolder> camera_image_mailbox_holder =
      base_layer->CameraImageMailboxHolder();

  if (!camera_image_mailbox_holder) {
    return nullptr;
  }

  GLuint texture_id = base_layer->CameraImageTextureId();

  // This resource is owned by the renderer, and is freed OnFrameEnd();
  WebGLUnownedTexture* texture = MakeGarbageCollected<WebGLUnownedTexture>(
      webgl_context_, texture_id, GL_TEXTURE_2D);
  return texture;
}

void XRWebGLBinding::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(webgl_context_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
