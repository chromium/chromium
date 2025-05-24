// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object.h"

namespace blink {

WebGLVertexArrayObject::WebGLVertexArrayObject(WebGLContextObjectSupport* ctx,
                                               VaoType type,
                                               GLint max_vertex_attribs)
    : WebGLVertexArrayObjectBase(ctx, type, max_vertex_attribs) {}

}  // namespace blink
