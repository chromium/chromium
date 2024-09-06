// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgl/webgl_shader_pixel_local_storage.h"

#include <array>
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

bool WebGLShaderPixelLocalStorage::Supported(
    WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_ANGLE_shader_pixel_local_storage");
}

const char* WebGLShaderPixelLocalStorage::ExtensionName() {
  return "WEBGL_shader_pixel_local_storage";
}

WebGLShaderPixelLocalStorage::WebGLShaderPixelLocalStorage(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context),
      coherent_(context->ExtensionsUtil()->SupportsExtension(
          "GL_ANGLE_shader_pixel_local_storage_coherent")) {
  context->EnableExtensionIfSupported("OES_draw_buffers_indexed");
  context->EnableExtensionIfSupported("EXT_color_buffer_float");
  context->EnableExtensionIfSupported("EXT_color_buffer_half_float");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_shader_pixel_local_storage");
  context->ContextGL()->GetIntegerv(GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE,
                                    &max_pls_planes_);
}

WebGLExtensionName WebGLShaderPixelLocalStorage::GetName() const {
  return kWebGLShaderPixelLocalStorageName;
}

bool WebGLShaderPixelLocalStorage::isCoherent() const {
  return coherent_;
}

WebGLFramebuffer* WebGLShaderPixelLocalStorage::ValidatePLSFramebuffer(
    WebGLRenderingContextBase* context,
    const char* function_name) {
  WebGLFramebuffer* framebuffer_binding =
      context->GetFramebufferBinding(GL_DRAW_FRAMEBUFFER);
  if (!framebuffer_binding || !framebuffer_binding->Object() ||
      framebuffer_binding->Opaque()) {
    context->SynthesizeGLError(
        GL_INVALID_OPERATION, function_name,
        "framebuffer does not support pixel local storage");
    return nullptr;
  }
  return framebuffer_binding;
}

bool WebGLShaderPixelLocalStorage::ValidatePLSPlaneIndex(
    WebGLRenderingContextBase* context,
    const char* function_name,
    GLint plane) {
  if (plane < 0) {
    context->SynthesizeGLError(GL_INVALID_VALUE, function_name,
                               "<plane> is < 0");
    return false;
  }
  if (plane >= max_pls_planes_) {
    context->SynthesizeGLError(
        GL_INVALID_VALUE, function_name,
        "<plane> is >= GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE");
    return false;
  }
  return true;
}

bool WebGLShaderPixelLocalStorage::ValidatePLSClearCommand(
    WebGLRenderingContextBase* context,
    const char* function_name,
    GLint plane,
    size_t src_length,
    GLuint src_offset) {
  if (!ValidatePLSFramebuffer(context, function_name) ||
      !ValidatePLSPlaneIndex(context, function_name, plane)) {
    return false;
  }
  if (!base::CheckAdd(src_offset, 4u).IsValid()) {
    context->SynthesizeGLError(GL_INVALID_VALUE, function_name,
                               "clear offset is too large");
    return false;
  }
  if (src_length < src_offset + 4u) {
    context->SynthesizeGLError(GL_INVALID_VALUE, function_name,
                               "clear value must contain at least 4 elements");
    return false;
  }
  return true;
}

void WebGLShaderPixelLocalStorage::framebufferTexturePixelLocalStorageWEBGL(
    GLint plane,
    WebGLTexture* texture,
    GLint level,
    GLint layer) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] =
      "framebufferTexturePixelLocalStorageWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  WebGLFramebuffer* framebuffer =
      ValidatePLSFramebuffer(context, function_name);
  if (!framebuffer) {
    return;
  }
  if (!ValidatePLSPlaneIndex(context, function_name, plane) ||
      !context->ValidateNullableWebGLObject(function_name, texture)) {
    return;
  }
  context->ContextGL()->FramebufferTexturePixelLocalStorageANGLE(
      plane, ObjectOrZero(texture), level, layer);
  framebuffer->SetPLSTexture(plane, texture);
}

void WebGLShaderPixelLocalStorage::framebufferPixelLocalClearValuefvWEBGL(
    GLint plane,
    base::span<const GLfloat> value,
    GLuint src_offset) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] =
      "framebufferPixelLocalClearValuefvWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSClearCommand(context, function_name, plane, value.size(),
                               src_offset)) {
    return;
  }
  context->ContextGL()->FramebufferPixelLocalClearValuefvANGLE(
      plane, value.data() + src_offset);
}

void WebGLShaderPixelLocalStorage::framebufferPixelLocalClearValueivWEBGL(
    GLint plane,
    base::span<const GLint> value,
    GLuint src_offset) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] =
      "framebufferPixelLocalClearValueivWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSClearCommand(context, function_name, plane, value.size(),
                               src_offset)) {
    return;
  }
  context->ContextGL()->FramebufferPixelLocalClearValueivANGLE(
      plane, value.data() + src_offset);
}

void WebGLShaderPixelLocalStorage::framebufferPixelLocalClearValueuivWEBGL(
    GLint plane,
    base::span<const GLuint> value,
    GLuint src_offset) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] =
      "framebufferPixelLocalClearValueuivWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSClearCommand(context, function_name, plane, value.size(),
                               src_offset)) {
    return;
  }
  context->ContextGL()->FramebufferPixelLocalClearValueuivANGLE(
      plane, value.data() + src_offset);
}

void WebGLShaderPixelLocalStorage::beginPixelLocalStorageWEBGL(
    const Vector<GLenum>& loadops) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] = "beginPixelLocalStorageWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSFramebuffer(context, function_name)) {
    return;
  }
  // Perform any deferred clears before we begin pixel local storage.
  context->ClearIfComposited(WebGLRenderingContextBase::kClearCallerOther);
  context->ContextGL()->BeginPixelLocalStorageANGLE(loadops.size(),
                                                    loadops.data());
  // Let the context know we have used pixel local storage so it will start
  // using the interrupt mechanism when it takes over the client's context.
  context->has_activated_pixel_local_storage_ = true;
}

void WebGLShaderPixelLocalStorage::endPixelLocalStorageWEBGL(
    const Vector<GLenum>& storeops) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] = "endPixelLocalStorageWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSFramebuffer(context, function_name)) {
    return;
  }
  context->ContextGL()->EndPixelLocalStorageANGLE(storeops.size(),
                                                  storeops.data());
}

void WebGLShaderPixelLocalStorage::pixelLocalStorageBarrierWEBGL() {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  constexpr static char function_name[] = "pixelLocalStorageBarrierWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  if (!ValidatePLSFramebuffer(context, function_name)) {
    return;
  }
  context->ContextGL()->PixelLocalStorageBarrierANGLE();
}

ScriptValue
WebGLShaderPixelLocalStorage::getFramebufferPixelLocalStorageParameterWEBGL(
    ScriptState* script_state,
    GLint plane,
    GLenum pname) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  constexpr static char function_name[] =
      "getFramebufferPixelLocalStorageParameterWEBGL";
  WebGLRenderingContextBase* context = scoped.Context();
  WebGLFramebuffer* framebuffer =
      ValidatePLSFramebuffer(context, function_name);
  if (!framebuffer) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  if (!ValidatePLSPlaneIndex(context, function_name, plane)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  gpu::gles2::GLES2Interface* gl = context->ContextGL();
  switch (pname) {
    case GL_PIXEL_LOCAL_FORMAT_ANGLE: {
      GLint value{};
      gl->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname, &value);
      return WebGLAny(script_state, static_cast<GLenum>(value));
    }
    case GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE: {
      DCHECK(framebuffer);
      WebGLTexture* tex = framebuffer->GetPLSTexture(plane);
      GLint attachedTextureID{};
      gl->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname,
                                                          &attachedTextureID);
      if (static_cast<GLuint>(attachedTextureID) != ObjectOrZero(tex)) {
        // Implementation gap! Tracked PLS texture is out of sync with actual.
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      return WebGLAny(script_state, tex);
    }
    case GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE:
    case GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE: {
      GLint value{};
      gl->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname, &value);
      return WebGLAny(script_state, value);
    }
    case GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE: {
      DOMFloat32Array* values = DOMFloat32Array::Create(4);
      gl->GetFramebufferPixelLocalStorageParameterfvANGLE(plane, pname,
                                                          values->Data());
      return WebGLAny(script_state, values);
    }
    case GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE: {
      DOMInt32Array* values = DOMInt32Array::Create(4);
      gl->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname,
                                                          values->Data());
      return WebGLAny(script_state, values);
    }
    case GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE: {
      DOMUint32Array* values = DOMUint32Array::Create(4);
      gl->GetFramebufferPixelLocalStorageParameterivANGLE(
          plane, pname, reinterpret_cast<GLint*>(values->Data()));
      return WebGLAny(script_state, values);
    }
  }
  return ScriptValue::CreateNull(script_state->GetIsolate());
}

}  // namespace blink
