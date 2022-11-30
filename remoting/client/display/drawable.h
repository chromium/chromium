// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_DRAWABLE_H_
#define REMOTING_CLIENT_DISPLAY_DRAWABLE_H_

#include "base/memory/weak_ptr.h"

namespace remoting {

class Canvas;

// Interface for drawing on a Canvas from a renderer.
class Drawable {
 public:
  Drawable() {}

  Drawable(const Drawable&) = delete;
  Drawable& operator=(const Drawable&) = delete;

  virtual ~Drawable() {}

  // Sets the canvas on which the object will be drawn.
  // If |canvas| is nullptr, nothing will happen when calling Draw().
  virtual void SetCanvas(base::WeakPtr<Canvas> canvas) = 0;

  // Draws the object on the canvas.
  // Returns true if there is a pending next frame.
  virtual bool Draw() = 0;

  // Used for the renderer to keep a stack of drawables.
  virtual base::WeakPtr<Drawable> GetWeakPtr() = 0;

  // ZIndex is a recommendation for Z Index of drawable components.
  enum ZIndex {
    DESKTOP = 100,
    CURSOR_FEEDBACK = 200,
    CURSOR = 300,
  };

  // A higher Z Index should be draw ontop of a lower z index. Elements with
  // the same Z Index should draw in order inserted into the renderer.
  virtual int GetZIndex() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_DRAWABLE_H_
