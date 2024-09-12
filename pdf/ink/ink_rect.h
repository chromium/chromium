// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_RECT_H_
#define PDF_INK_INK_RECT_H_

namespace chrome_pdf {

// A simpler version of Ink's Rect class.
struct InkRect {
  constexpr InkRect() = default;
  constexpr InkRect(float x_min, float y_min, float x_max, float y_max)
      : x_min(x_min), y_min(y_min), x_max(x_max), y_max(y_max) {}
  ~InkRect() = default;

  float x_min = 0;
  float y_min = 0;
  float x_max = 0;
  float y_max = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_RECT_H_
