// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FENCE_SYNC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FENCE_SYNC_H_

#include "third_party/blink/renderer/modules/webgl/webgl_sync.h"

namespace blink {

class WebGLFenceSync : public WebGLSync {
 public:
  WebGLFenceSync(WebGLContextObjectSupport*,
                 GLenum condition,
                 GLbitfield flags);

 private:
  GLuint insertQuery(WebGLContextObjectSupport*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FENCE_SYNC_H_
