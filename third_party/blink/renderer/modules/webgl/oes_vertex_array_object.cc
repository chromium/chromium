/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/oes_vertex_array_object.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object_oes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_OES_vertex_array_object");
}

WebGLExtensionName OESVertexArrayObject::GetName() const {
  return kOESVertexArrayObjectName;
}

WebGLVertexArrayObjectOES* OESVertexArrayObject::createVertexArrayOES() {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return nullptr;

  return MakeGarbageCollected<WebGLVertexArrayObjectOES>(
      scoped.Context(), WebGLVertexArrayObjectOES::kVaoTypeUser);
}

void OESVertexArrayObject::deleteVertexArrayOES(
    WebGLVertexArrayObjectOES* array_object) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() || !array_object)
    return;

  // ValidateWebGLObject generates an error if the object has already been
  // deleted, so we must replicate most of its checks here.
  if (!array_object->Validate(scoped.Context()->ContextGroup(),
                              scoped.Context())) {
    scoped.Context()->SynthesizeGLError(
        GL_INVALID_OPERATION, "deleteVertexArrayOES",
        "object does not belong to this context");
    return;
  }

  if (array_object->MarkedForDeletion())
    return;

  if (!array_object->IsDefaultObject() &&
      array_object == scoped.Context()->bound_vertex_array_object_)
    scoped.Context()->SetBoundVertexArrayObject(nullptr);

  array_object->DeleteObject(scoped.Context()->ContextGL());
}

bool OESVertexArrayObject::isVertexArrayOES(
    WebGLVertexArrayObjectOES* array_object) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() || !array_object ||
      !array_object->Validate(scoped.Context()->ContextGroup(),
                              scoped.Context()))
    return false;

  if (!array_object->HasEverBeenBound())
    return false;
  if (array_object->MarkedForDeletion())
    return false;

  return scoped.Context()->ContextGL()->IsVertexArrayOES(
      array_object->Object());
}

void OESVertexArrayObject::bindVertexArrayOES(
    WebGLVertexArrayObjectOES* array_object) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  if (!scoped.Context()->ValidateNullableWebGLObject(
          "OESVertexArrayObject.bindVertexArrayOES", array_object))
    return;

  if (array_object && !array_object->IsDefaultObject() &&
      array_object->Object()) {
    scoped.Context()->ContextGL()->BindVertexArrayOES(array_object->Object());

    array_object->SetHasEverBeenBound();
    scoped.Context()->SetBoundVertexArrayObject(array_object);
  } else {
    scoped.Context()->ContextGL()->BindVertexArrayOES(0);
    scoped.Context()->SetBoundVertexArrayObject(nullptr);
  }
}

bool OESVertexArrayObject::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_vertex_array_object");
}

const char* OESVertexArrayObject::ExtensionName() {
  return "OES_vertex_array_object";
}

}  // namespace blink
