// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_STROKE_H_
#define PDF_INK_INK_STROKE_H_

#include <memory>

#include "pdf/ink/ink_modeled_shape_view.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

class InkBrush;
class InkStrokeInputBatch;
class InkStrokeInputBatchView;

class InkStroke {
 public:
  // Create a stroke for use in testing.  The `groups_outlines` and `bounds`
  // parameters are needed only by the stub implementation; they will be
  // ignored by the wrapper, which can generate a real modeled shape using
  // `inputs`.
  // TODO(crbug.com/339682315):  Remove the parameters used only by the stub
  // implementation once the wrapper is fully available.
  static std::unique_ptr<InkStroke> CreateForTesting(
      const InkBrush& brush,
      const InkStrokeInputBatch& inputs,
      const InkModeledShapeView::GroupsOutlines& groups_outlines,
      const InkRect& bounds);

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
