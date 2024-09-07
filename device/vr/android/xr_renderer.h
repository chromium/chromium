// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_XR_RENDERER_H_
#define DEVICE_VR_ANDROID_XR_RENDERER_H_

#include "base/component_export.h"
#include "device/vr/android/local_texture.h"
#include "ui/gl/gl_bindings.h"

namespace device {

// Issues GL for rendering a texture for WebXr.
class XrRenderer {
 public:
  XrRenderer();

  XrRenderer(const XrRenderer&) = delete;
  XrRenderer& operator=(const XrRenderer&) = delete;

  ~XrRenderer();

  // Blits the provided texture handle onto the currently bound framebuffer,
  // applying the provided uv_transform.
  void Draw(const LocalTexture& texture, const float (&uv_transform)[16]);

 private:
  struct Program {
    GLuint program_handle_ = 0;
    GLuint position_handle_ = 0;
    GLuint texture_handle_ = 0;
    GLuint uv_transform_ = 0;
  };

  Program CreateProgram(const std::string& vertex, const std::string& fragment);

  Program program_external_;
  Program program_2d_;

  GLuint vertex_buffer_ = 0;
  GLuint index_buffer_ = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_RENDERER_H_
