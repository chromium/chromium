// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_STROKE_H_
#define PDF_INK_INK_STROKE_H_

#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

class InkModeledShapeView;
class InkStrokeInputBatchView;

class InkStroke {
 public:
  virtual ~InkStroke() = default;

  // Get the color used with the brush for this stroke.  Ink's API allows
  // access to the entire ink brush from a stroke; this is a simplication of
  // that, to allow only getting the color of the brush used.
  virtual SkColor GetBrushColor() const = 0;

  virtual const InkStrokeInputBatchView& GetInputs() const = 0;

  virtual const InkModeledShapeView& GetShape() const = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_STROKE_H_
