// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_instanced_base_vertex_base_instance.h"

#include "gpu/command_buffer/client/gles2_interface.h"

namespace blink {

WebGLMultiDrawInstancedBaseVertexBaseInstance::
    WebGLMultiDrawInstancedBaseVertexBaseInstance(
        WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_WEBGL_multi_draw_instanced_base_vertex_base_instance");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_base_vertex_base_instance");
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_WEBGL_multi_draw");
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_ANGLE_multi_draw");
}

WebGLMultiDrawInstancedBaseVertexBaseInstance*
WebGLMultiDrawInstancedBaseVertexBaseInstance::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<WebGLMultiDrawInstancedBaseVertexBaseInstance>(
      context);
}

WebGLExtensionName WebGLMultiDrawInstancedBaseVertexBaseInstance::GetName()
    const {
  return kWebGLMultiDrawInstancedBaseVertexBaseInstanceName;
}

bool WebGLMultiDrawInstancedBaseVertexBaseInstance::Supported(
    WebGLRenderingContextBase* context) {
  // Logic: IsSupportedByValidating || IsSupportedByPassthroughOnANGLE
  // GL_ANGLE_base_vertex_base_instance is removed from supports if we requested
  // GL_WEBGL_draw_instanced_base_vertex_base_instance first
  // So we need to add a or for
  // GL_WEBGL_draw_instanced_base_vertex_base_instance
  return (context->ExtensionsUtil()->SupportsExtension(
              "GL_WEBGL_draw_instanced_base_vertex_base_instance") &&
          context->ExtensionsUtil()->SupportsExtension(
              "GL_WEBGL_multi_draw_instanced_base_vertex_base_instance")) ||
         (context->ExtensionsUtil()->SupportsExtension("GL_ANGLE_multi_draw") &&
          (context->ExtensionsUtil()->SupportsExtension(
               "GL_ANGLE_base_vertex_base_instance") ||
           context->ExtensionsUtil()->SupportsExtension(
               "GL_WEBGL_draw_instanced_base_vertex_base_instance")));
}

const char* WebGLMultiDrawInstancedBaseVertexBaseInstance::ExtensionName() {
  return "WEBGL_multi_draw_instanced_base_vertex_base_instance";
}

void WebGLMultiDrawInstancedBaseVertexBaseInstance::
    multiDrawArraysInstancedBaseInstanceImpl(
        GLenum mode,
        const base::span<const int32_t> firsts,
        GLuint firsts_offset,
        const base::span<const int32_t> counts,
        GLuint counts_offset,
        const base::span<const int32_t> instance_counts,
        GLuint instance_counts_offset,
        const base::span<const int32_t> baseinstances,
        GLuint baseinstances_offset,
        GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(&scoped, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
                         drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
                     "firstsOffset out of bounds", firsts.size(), firsts_offset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
                     "countsOffset out of bounds", counts.size(), counts_offset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
                     "instanceCountsOffset out of bounds",
                     instance_counts.size(), instance_counts_offset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
                     "baseinstancesOffset out of bounds", baseinstances.size(),
                     baseinstances_offset, drawcount)) {
    return;
  }

  scoped.Context()->ContextGL()->MultiDrawArraysInstancedBaseInstanceWEBGL(
      mode, &firsts[firsts_offset], &counts[counts_offset],
      &instance_counts[instance_counts_offset],
      reinterpret_cast<const GLuint*>(&baseinstances[baseinstances_offset]),
      drawcount);
}

void WebGLMultiDrawInstancedBaseVertexBaseInstance::
    multiDrawElementsInstancedBaseVertexBaseInstanceImpl(
        GLenum mode,
        const base::span<const int32_t> counts,
        GLuint counts_offset,
        GLenum type,
        const base::span<const int32_t> offsets,
        GLuint offsets_offset,
        const base::span<const int32_t> instance_counts,
        GLuint instance_counts_offset,
        const base::span<const int32_t> basevertices,
        GLuint basevertices_offset,
        const base::span<const int32_t> baseinstances,
        GLuint baseinstances_offset,
        GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(
          &scoped, "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
          drawcount) ||
      !ValidateArray(&scoped,
                     "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
                     "countsOffset out of bounds", counts.size(), counts_offset,
                     drawcount) ||
      !ValidateArray(&scoped,
                     "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
                     "offsetsOffset out of bounds", offsets.size(),
                     offsets_offset, drawcount) ||
      !ValidateArray(
          &scoped, "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
          "instanceCountsOffset out of bounds", instance_counts.size(),
          instance_counts_offset, drawcount) ||
      !ValidateArray(&scoped,
                     "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
                     "baseverticesOffset out of bounds", basevertices.size(),
                     basevertices_offset, drawcount) ||
      !ValidateArray(&scoped,
                     "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
                     "baseinstancesOffset out of bounds", baseinstances.size(),
                     baseinstances_offset, drawcount)) {
    return;
  }

  scoped.Context()
      ->ContextGL()
      ->MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
          mode, &counts[counts_offset], type, &offsets[offsets_offset],
          &instance_counts[instance_counts_offset],
          &basevertices[basevertices_offset],
          reinterpret_cast<const GLuint*>(&baseinstances[baseinstances_offset]),
          drawcount);
}

}  // namespace blink
