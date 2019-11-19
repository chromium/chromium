/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/angle_instanced_arrays.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

ANGLEInstancedArrays::ANGLEInstancedArrays(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_instanced_arrays");
}

WebGLExtensionName ANGLEInstancedArrays::GetName() const {
  return kANGLEInstancedArraysName;
}

ANGLEInstancedArrays* ANGLEInstancedArrays::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<ANGLEInstancedArrays>(context);
}

bool ANGLEInstancedArrays::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_ANGLE_instanced_arrays");
}

const char* ANGLEInstancedArrays::ExtensionName() {
  return "ANGLE_instanced_arrays";
}

void ANGLEInstancedArrays::drawArraysInstancedANGLE(GLenum mode,
                                                    GLint first,
                                                    GLsizei count,
                                                    GLsizei primcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  scoped.Context()->DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void ANGLEInstancedArrays::drawElementsInstancedANGLE(GLenum mode,
                                                      GLsizei count,
                                                      GLenum type,
                                                      int64_t offset,
                                                      GLsizei primcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  scoped.Context()->DrawElementsInstancedANGLE(mode, count, type, offset,
                                               primcount);
}

void ANGLEInstancedArrays::vertexAttribDivisorANGLE(GLuint index,
                                                    GLuint divisor) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  scoped.Context()->VertexAttribDivisorANGLE(index, divisor);
}

}  // namespace blink
