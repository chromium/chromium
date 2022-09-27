// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHARED_PLATFORM_3D_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHARED_PLATFORM_3D_OBJECT_H_

#include "third_party/blink/renderer/modules/webgl/webgl_shared_object.h"

namespace blink {

class WebGLRenderingContextBase;

class WebGLSharedPlatform3DObject : public WebGLSharedObject {
 public:
  GLuint Object() const { return object_; }
  void SetObject(GLuint);

 protected:
  explicit WebGLSharedPlatform3DObject(WebGLRenderingContextBase*);

  bool HasObject() const override;

  GLuint object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHARED_PLATFORM_3D_OBJECT_H_
