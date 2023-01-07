/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_draw_instanced_base_vertex_base_instance.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLDrawInstancedBaseVertexBaseInstance::
    WebGLDrawInstancedBaseVertexBaseInstance(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_WEBGL_draw_instanced_base_vertex_base_instance");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_base_vertex_base_instance");
}

WebGLExtensionName WebGLDrawInstancedBaseVertexBaseInstance::GetName() const {
  return kWebGLDrawInstancedBaseVertexBaseInstanceName;
}

// static
bool WebGLDrawInstancedBaseVertexBaseInstance::Supported(
    WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
             "GL_WEBGL_draw_instanced_base_vertex_base_instance") ||
         context->ExtensionsUtil()->SupportsExtension(
             "GL_ANGLE_base_vertex_base_instance");
}

// static
const char* WebGLDrawInstancedBaseVertexBaseInstance::ExtensionName() {
  return "WEBGL_draw_instanced_base_vertex_base_instance";
}

void WebGLDrawInstancedBaseVertexBaseInstance::
    drawArraysInstancedBaseInstanceWEBGL(GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instance_count,
                                         GLuint baseinstance) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  scoped.Context()->DrawWrapper(
      "drawArraysInstancedBaseInstanceWEBGL",
      CanvasPerformanceMonitor::DrawType::kDrawArrays, [&]() {
        scoped.Context()->ContextGL()->DrawArraysInstancedBaseInstanceANGLE(
            mode, first, count, instance_count, baseinstance);
      });
}

void WebGLDrawInstancedBaseVertexBaseInstance::
    drawElementsInstancedBaseVertexBaseInstanceWEBGL(GLenum mode,
                                                     GLsizei count,
                                                     GLenum type,
                                                     GLintptr offset,
                                                     GLsizei instance_count,
                                                     GLint basevertex,
                                                     GLuint baseinstance) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  scoped.Context()->DrawWrapper(
      "drawElementsInstancedBaseVertexBaseInstanceWEBGL",
      CanvasPerformanceMonitor::DrawType::kDrawElements, [&]() {
        scoped.Context()
            ->ContextGL()
            ->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
                mode, count, type, reinterpret_cast<void*>(offset),
                instance_count, basevertex, baseinstance);
      });
}

}  // namespace blink
