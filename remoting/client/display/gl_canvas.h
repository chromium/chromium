// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_CANVAS_H_
#define REMOTING_CLIENT_DISPLAY_GL_CANVAS_H_

#include <array>

#include "base/threading/thread_checker.h"
#include "remoting/client/display/canvas.h"
#include "remoting/client/display/sys_opengl.h"

namespace remoting {

// This class holds zoom and pan configurations of the canvas and is used to
// draw textures on the canvas.
// Must be constructed after the OpenGL surface is created and destroyed before
// the surface is destroyed.
class GlCanvas : public Canvas {
 public:
  // gl_version: version number of the OpenGL ES context. Either 2 or 3.
  GlCanvas(int gl_version);

  GlCanvas(const GlCanvas&) = delete;
  GlCanvas& operator=(const GlCanvas&) = delete;

  ~GlCanvas() override;

  // Canvas implementation.
  void Clear() override;
  void SetTransformationMatrix(const std::array<float, 9>& matrix) override;
  void SetViewSize(int width, int height) override;
  void DrawTexture(int texture_id,
                   int texture_handle,
                   int vertex_buffer,
                   float alpha_multiplier) override;
  int GetVersion() const override;
  int GetMaxTextureSize() const override;
  base::WeakPtr<Canvas> GetWeakPtr() override;

 private:
  int gl_version_;

  int max_texture_size_ = 0;
  bool transformation_set_ = false;
  bool view_size_set_ = false;

  // Handles.
  GLuint vertex_shader_;
  GLuint fragment_shader_;
  GLuint program_;

  // Locations of the corresponding shader attributes.
  GLuint transform_location_;
  GLuint view_size_location_;
  GLuint texture_location_;
  GLuint alpha_multiplier_location_;
  GLuint position_location_;
  GLuint tex_cord_location_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Canvas> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_CANVAS_H_
