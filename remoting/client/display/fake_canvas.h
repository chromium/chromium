// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_FAKE_CANVAS_H_
#define REMOTING_CLIENT_DISPLAY_FAKE_CANVAS_H_

#include <array>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/sys_opengl.h"

namespace remoting {

class FakeCanvas : public Canvas {
 public:
  FakeCanvas();
  ~FakeCanvas() override;

  // Drawable implementation.
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
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Canvas> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCanvas);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_FAKE_CANVAS_H_
