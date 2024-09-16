// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_stub.h"

#include "base/check_is_test.h"
#include "pdf/ink/ink_brush.h"
#include "pdf/ink/ink_rect.h"
#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_view_stub.h"

namespace chrome_pdf {

InkStrokeStub::InkStrokeStub(SkColor brush_color,
                             const InkStrokeInputBatchStub& inputs)
    : brush_color_(brush_color),
      shape_(InkModeledShapeViewStub(InkModeledShapeView::GroupsOutlines(),
                                     InkRect())),
      inputs_(inputs),
      inputs_view_(inputs_) {}

InkStrokeStub::InkStrokeStub(
    SkColor brush_color,
    const InkStrokeInputBatchStub& inputs,
    const InkModeledShapeView::GroupsOutlines& groups_outlines,
    const InkRect& bounds)
    : brush_color_(brush_color),
      shape_(InkModeledShapeViewStub(groups_outlines, bounds)),
      inputs_(inputs),
      inputs_view_(inputs_) {
  CHECK_IS_TEST();
}

InkStrokeStub::~InkStrokeStub() = default;

SkColor InkStrokeStub::GetBrushColor() const {
  return brush_color_;
}

const InkStrokeInputBatchView& InkStrokeStub::GetInputs() const {
  return inputs_view_;
}

const InkModeledShapeView& InkStrokeStub::GetShape() const {
  return shape_;
}

// static
std::unique_ptr<InkStroke> InkStroke::CreateForTesting(
    const InkBrush& brush,
    const InkStrokeInputBatch& inputs,
    const InkModeledShapeView::GroupsOutlines& groups_outlines,
    const InkRect& bounds) {
  return std::make_unique<InkStrokeStub>(
      brush.GetColor(), static_cast<const InkStrokeInputBatchStub&>(inputs),
      groups_outlines, bounds);
}

}  // namespace chrome_pdf
