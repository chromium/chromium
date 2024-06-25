// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_RECT_H_
#define PDF_INK_INK_RECT_H_

namespace chrome_pdf {

// A simpler version of Ink's Rect class.
struct InkRect {
  float x_min;
  float y_min;
  float x_max;
  float y_max;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_RECT_H_
