// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_AR_RENDERER_H_
#define DEVICE_VR_ANDROID_ARCORE_AR_RENDERER_H_

#include "ui/gl/gl_bindings.h"

namespace device {

// Issues GL for rendering a texture for AR.
// TODO(crbug.com/838013): Share code with WebVrRenderer.
class ArRenderer {
 public:
  ArRenderer();

  ArRenderer(const ArRenderer&) = delete;
  ArRenderer& operator=(const ArRenderer&) = delete;

  ~ArRenderer();

  void Draw(int texture_handle, const float (&uv_transform)[16]);

 private:
  GLuint program_handle_ = 0;
  GLuint position_handle_ = 0;
  GLuint clip_rect_handle_ = 0;
  GLuint texture_handle_ = 0;
  GLuint uv_transform_ = 0;
  GLuint vertex_buffer_ = 0;
  GLuint index_buffer_ = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_AR_RENDERER_H_
