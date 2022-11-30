// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_CANVAS_H_
#define REMOTING_CLIENT_DISPLAY_CANVAS_H_

#include <array>

#include "base/memory/weak_ptr.h"

namespace remoting {

// This class holds zoom and pan configurations of the canvas and is used to
// draw textures on the canvas.
class Canvas {
 public:
  Canvas() {}

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  virtual ~Canvas() {}

  // Clears the frame.
  virtual void Clear() = 0;

  // Sets the transformation matrix. This matrix defines how the canvas should
  // be shown on the view.
  // 3 by 3 transformation matrix, [ m0, m1, m2, m3, m4, m5, m6, m7, m8 ].
  // The matrix will be multiplied with the positions (with projective space,
  // (x, y, 1)) to draw the textures with the right zoom and pan configuration.
  //
  // | m0, m1, m2, |   | x |
  // | m3, m4, m5, | * | y |
  // | m6, m7, m8  |   | 1 |
  //
  // For a typical transformation matrix such that m1=m3=m6=m7=0 and m8=1, m0
  // and m4 defines the scaling factor of the canvas and m2 and m5 defines the
  // offset of the upper-left corner in pixel.
  virtual void SetTransformationMatrix(const std::array<float, 9>& matrix) = 0;

  // Sets the size of the view in pixels such that it fills up the the whole
  // viewport.
  // Note that this only affects the transformation matrix. It doesn't affect
  // how the viewport is rendered on the screen.
  virtual void SetViewSize(int width, int height) = 0;

  // Draws the texture on the canvas. Nothing will happen if
  // SetNormalizedTransformation() has not been called.
  // vertex_buffer: reference to the 2x4x2 float vertex buffer.
  //                [ four (x, y) position of the texture vertices in pixel
  //                              with respect to the canvas,
  //                  four (x, y) position of the vertices in percentage
  //                              defining the visible area of the texture ]
  // alpha_multiplier: Will be multiplied with the alpha channel of the texture.
  //                   Passing 1 means no change of the transparency of the
  //                   texture.
  virtual void DrawTexture(int texture_id,
                           int texture_handle,
                           int vertex_buffer,
                           float alpha_multiplier) = 0;

  // Version if applicable to implementation. Default 0 if unused.
  virtual int GetVersion() const = 0;

  // Returns the maximum texture resolution limitation. Neither the width nor
  // the height of the texture can exceed this limitation.
  virtual int GetMaxTextureSize() const = 0;

  // Intended to be given to a Drawable to draw onto.
  virtual base::WeakPtr<Canvas> GetWeakPtr() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_CANVAS_H_
