// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_XR_RENDERER_H_
#define DEVICE_VR_ANDROID_XR_RENDERER_H_

#include "base/component_export.h"
#include "device/vr/android/local_texture.h"
#include "ui/gfx/geometry/rect_f.h"
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
  // applying the provided uv_transform and opacity.
  void Draw(const LocalTexture& texture,
            const float (&uv_transform)[16],
            float opacity = 1.f);

  // Blits the 6 faces (3x2) from the provided texture onto the target
  // cubemap texture, applying the provided uv_transform and opacity.
  void DrawCubemap(const LocalTexture& texture,
                   uint32_t target_texture,
                   const float (&uv_transform)[16],
                   float opacity = 1.f);

  // Blits the layers of the source texture array onto the layers of
  // the target array, applying the provided uv_transform and opacity.
  void DrawArray(const LocalTexture& texture,
                 uint32_t target_texture,
                 uint16_t layers,
                 const float (&uv_transform)[16],
                 float opacity = 1.f);

  // Blits the specified layer of the source texture array onto the currently
  // bound framebuffer, applying the provided uv_transform and opacity.
  void DrawArrayLayer(const LocalTexture& texture,
                      uint16_t layer_index,
                      const float (&uv_transform)[16],
                      float opacity = 1.f);

  struct OverlayLayerParams {
    LocalTexture texture;
    // Normalized [0, 1] UV bounding box in the target eye layer. An empty
    // RectF skips overlay sampling.
    gfx::RectF bounds;
    // Horizontal UV offset into the side-by-side stereo overlay texture (0.0f
    // for the left eye, 0.5f for the right eye).
    float eye_uv_offset = 0.0f;
    bool flip_y = false;
  };

  // Blits the specified layer of the source texture array and composites the
  // 2D overlay texture within `overlay.bounds` in a single unblended pass.
  void DrawArrayLayerWithOverlay(const LocalTexture& texture,
                                 uint16_t layer_index,
                                 const float (&uv_transform)[16],
                                 float opacity,
                                 const OverlayLayerParams& overlay);

 private:
  struct Program {
    GLuint program_handle_ = 0;
    GLuint position_handle_ = 0;
    GLuint texture_handle_ = 0;
    GLuint overlay_texture_handle_ = 0;
    GLuint uv_transform_ = 0;
    GLuint opacity_ = 0;
    GLuint column_index_ = 0;
    GLuint row_index_ = 0;
    GLuint layer_index_ = 0;
    GLuint overlay_bounds_ = 0;
    GLuint overlay_uv_transform_ = 0;
  };

  Program CreateProgram(const std::string& vertex, const std::string& fragment);
  void EnsureVertexBuffers();
  void Draw(const Program& program,
            const LocalTexture& texture,
            const float (&uv_transform)[16],
            float opacity,
            int face_index = -1,
            int layer_index = -1);

  Program program_external_;
  Program program_2d_;
  Program program_cubemap_;
  Program program_array_;
  Program program_array_overlay_;

  GLuint vertex_buffer_ = 0;
  GLuint index_buffer_ = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_RENDERER_H_
