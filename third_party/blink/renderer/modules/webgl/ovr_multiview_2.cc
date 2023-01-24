// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ovr_multiview_2.h"

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

OVRMultiview2::OVRMultiview2(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_OVR_multiview2");
  context->ContextGL()->GetIntegerv(GL_MAX_VIEWS_OVR, &max_views_ovr_);
}

WebGLExtensionName OVRMultiview2::GetName() const {
  return kOVRMultiview2Name;
}

void OVRMultiview2::framebufferTextureMultiviewOVR(GLenum target,
                                                   GLenum attachment,
                                                   WebGLTexture* texture,
                                                   GLint level,
                                                   GLint base_view_index,
                                                   GLsizei num_views) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  if (!scoped.Context()->ValidateNullableWebGLObject(
          "framebufferTextureMultiviewOVR", texture))
    return;
  GLenum textarget = texture ? texture->GetTarget() : 0;
  if (texture) {
    if (textarget != GL_TEXTURE_2D_ARRAY) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                          "framebufferTextureMultiviewOVR",
                                          "invalid texture type");
      return;
    }
    if (num_views < 1) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_VALUE,
                                          "framebufferTextureMultiviewOVR",
                                          "numViews is less than one");
      return;
    }
    if (num_views > max_views_ovr_) {
      scoped.Context()->SynthesizeGLError(
          GL_INVALID_VALUE, "framebufferTextureMultiviewOVR",
          "numViews is more than the value of MAX_VIEWS_OVR");
      return;
    }
    if (!static_cast<WebGL2RenderingContextBase*>(scoped.Context())
             ->ValidateTexFuncLayer("framebufferTextureMultiviewOVR", textarget,
                                    base_view_index))
      return;
    if (!static_cast<WebGL2RenderingContextBase*>(scoped.Context())
             ->ValidateTexFuncLayer("framebufferTextureMultiviewOVR", textarget,
                                    base_view_index + num_views - 1))
      return;
    if (!scoped.Context()->ValidateTexFuncLevel(
            "framebufferTextureMultiviewOVR", textarget, level))
      return;
  }

  WebGLFramebuffer* framebuffer_binding =
      scoped.Context()->GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                        "framebufferTextureMultiviewOVR",
                                        "no framebuffer bound");
    return;
  }

  framebuffer_binding->SetAttachmentForBoundFramebuffer(
      target, attachment, textarget, texture, level, base_view_index,
      num_views);
  scoped.Context()->ApplyDepthAndStencilTest();
}

bool OVRMultiview2::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_OVR_multiview2");
}

const char* OVRMultiview2::ExtensionName() {
  return "OVR_multiview2";
}

}  // namespace blink
