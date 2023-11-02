// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_RENDER_LAYER_H_
#define REMOTING_CLIENT_DISPLAY_GL_RENDER_LAYER_H_

#include <array>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/sys_opengl.h"

namespace remoting {

class Canvas;

// This class is for drawing a texture on the canvas. Must be deleted before the
// canvas is deleted.
class GlRenderLayer {
 public:
  static const int kBytesPerPixel = 4;

  // texture_id: An integer in range [0, GL_MAX_TEXTURE_IMAGE_UNITS], defining
  //             which slot to store the texture.
  GlRenderLayer(int texture_id, base::WeakPtr<Canvas> canvas);

  GlRenderLayer(const GlRenderLayer&) = delete;
  GlRenderLayer& operator=(const GlRenderLayer&) = delete;

  ~GlRenderLayer();

  // Sets the texture (RGBA 8888) to be drawn. Please use UpdateTexture() if the
  // texture size isn't changed.
  // stride: byte distance between two rows in |subtexture|.
  // If |stride| is 0 or |stride| == |width|*kBytesPerPixel, |subtexture| will
  // be treated as tightly packed.
  void SetTexture(const uint8_t* texture, int width, int height, int stride);

  // Updates a subregion (RGBA 8888) of the texture. Can only be called after
  // SetTexture() has been called.
  // stride: See SetTexture().
  void UpdateTexture(const uint8_t* subtexture,
                     int offset_x,
                     int offset_y,
                     int width,
                     int height,
                     int stride);

  // Sets the positions of four vertices of the texture in pixel with respect to
  // the canvas.
  // positions: [ x_upperleft, y_upperleft, x_lowerleft, y_lowerleft,
  //              x_upperright, y_upperright, x_lowerright, y_lowerright ]
  void SetVertexPositions(const std::array<float, 8>& positions);

  // Sets the visible area of the texture in percentage of the width and height
  // of the texture. The default values are (0, 0), (0, 1), (1, 0), (1, 1),
  // i.e. showing the whole texture.
  // positions: [ x_upperleft, y_upperleft, x_lowerleft, y_lowerleft,
  //              x_upperright, y_upperright, x_lowerright, y_lowerright ]
  void SetTextureVisibleArea(const std::array<float, 8>& positions);

  // Draws the texture on the canvas. Texture must be set before calling Draw().
  void Draw(float alpha_multiplier);

  // true if the texture is already set by calling SetTexture().
  bool is_texture_set() { return texture_set_; }

 private:
  // Returns pointer to the texture buffer that can be used by glTexImage2d or
  // glTexSubImage2d. The returned value will be |data| if the texture can be
  // used without manual packing, otherwise the data will be manually packed and
  // the pointer to |update_buffer_| will be returned.
  // should_reset_row_length: Pointer to a bool that will be set by the
  //                          function. If this is true, the user need to call
  //                          glPixelStorei(GL_UNPACK_ROW_LENGTH, 0) after
  //                          updating the texture to reset the stride.
  const uint8_t* PrepareTextureBuffer(const uint8_t* data,
                                      int width,
                                      int height,
                                      int stride,
                                      bool* should_reset_row_length);

  int texture_id_;
  base::WeakPtr<Canvas> canvas_;

  GLuint texture_handle_;
  GLuint buffer_handle_;

  bool texture_set_ = false;

  bool vertex_position_set_ = false;

  // Used in OpenGL ES 2 context which doesn't support GL_UNPACK_ROW_LENGTH to
  // tightly pack dirty regions before sending them to GPU.
  std::unique_ptr<uint8_t[]> update_buffer_;
  int update_buffer_size_ = 0;

  base::ThreadChecker thread_checker_;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DISPLAY_GL_RENDER_LAYER_H_
