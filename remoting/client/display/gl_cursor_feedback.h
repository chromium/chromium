// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_H_
#define REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_H_

#include <cstdint>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "remoting/client/display/drawable.h"

namespace remoting {

class Canvas;
class GlRenderLayer;

// This class draws the cursor feedback on the canvas.
class GlCursorFeedback : public Drawable {
 public:
  GlCursorFeedback();

  GlCursorFeedback(const GlCursorFeedback&) = delete;
  GlCursorFeedback& operator=(const GlCursorFeedback&) = delete;

  ~GlCursorFeedback() override;

  void StartAnimation(float x, float y, float diameter);

  // Drawable implementation.
  void SetCanvas(base::WeakPtr<Canvas> canvas) override;
  bool Draw() override;
  int GetZIndex() override;
  base::WeakPtr<Drawable> GetWeakPtr() override;

 private:
  std::unique_ptr<GlRenderLayer> layer_;
  float max_diameter_ = 0;
  float cursor_x_ = 0;
  float cursor_y_ = 0;
  base::TimeTicks animation_start_time_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Drawable> weak_factory_{this};
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_H_
