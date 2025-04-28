// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_shared_platform_3d_object.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLSharedPlatform3DObject::WebGLSharedPlatform3DObject(
    WebGLRenderingContextBase* ctx)
    : WebGLSharedObject(ctx), object_(0) {}

void WebGLSharedPlatform3DObject::SetObject(GLuint object) {
  // SetObject may only be called when this container is in the
  // uninitialized state: object==0 && marked_for_deletion==false.
  DCHECK(!object_);
  DCHECK(!MarkedForDeletion());
  object_ = object;
}

bool WebGLSharedPlatform3DObject::HasObject() const {
  return object_ != 0;
}

}  // namespace blink
