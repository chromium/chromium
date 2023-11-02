// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_CURSOR_H_
#define REMOTING_CLIENT_DISPLAY_GL_CURSOR_H_

#include <array>
#include <cstdint>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/drawable.h"

namespace remoting {

namespace protocol {
class CursorShapeInfo;
}  // namespace protocol

class Canvas;
class GlRenderLayer;

// This class draws the cursor on the canvas.
class GlCursor : public Drawable {
 public:
  GlCursor();

  GlCursor(const GlCursor&) = delete;
  GlCursor& operator=(const GlCursor&) = delete;

  ~GlCursor() override;

  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape);

  // Sets the cursor hotspot positions. Does nothing if the cursor shape or the
  // canvas size has not been set.
  void SetCursorPosition(float x, float y);

  // Draw() will do nothing if cursor is not visible.
  void SetCursorVisible(bool visible);

  // Drawable implementation.
  void SetCanvas(base::WeakPtr<Canvas> canvas) override;
  bool Draw() override;
  int GetZIndex() override;
  base::WeakPtr<Drawable> GetWeakPtr() override;

 private:
  void SetCurrentCursorShape(bool size_changed);

  bool visible_ = true;

  std::unique_ptr<GlRenderLayer> layer_;

  std::unique_ptr<uint8_t[]> current_cursor_data_;
  int current_cursor_data_size_ = 0;
  int current_cursor_width_ = 0;
  int current_cursor_height_ = 0;
  int current_cursor_hotspot_x_ = 0;
  int current_cursor_hotspot_y_ = 0;

  float cursor_x_ = 0;
  float cursor_y_ = 0;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Drawable> weak_factory_{this};
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DISPLAY_GL_CURSOR_H_
