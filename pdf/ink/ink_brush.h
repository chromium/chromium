// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_H_
#define PDF_INK_INK_BRUSH_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

class InkBrushFamily;

class InkBrush {
 public:
  static std::unique_ptr<InkBrush> Create(
      std::unique_ptr<InkBrushFamily> family,
      SkColor color,
      float size,
      float epsilon);

  ~InkBrush();

 protected:
  InkBrush();
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_H_
