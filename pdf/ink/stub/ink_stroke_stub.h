// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_STROKE_STUB_H_
#define PDF_INK_STUB_INK_STROKE_STUB_H_

#include "pdf/ink/ink_stroke.h"
#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_view_stub.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

class InkModeledShapeViewStub;

class InkStrokeStub : public InkStroke {
 public:
  InkStrokeStub(SkColor brush_color, const InkStrokeInputBatchStub& inputs);
  InkStrokeStub(const InkStrokeStub&) = delete;
  InkStrokeStub& operator=(const InkStrokeStub&) = delete;
  ~InkStrokeStub() override;

  // InkStroke:
  SkColor GetBrushColor() const override;
  const InkStrokeInputBatchView& GetInputs() const override;
  const InkModeledShapeView& GetShape() const override;

 private:
  const SkColor brush_color_;
  const InkModeledShapeViewStub shape_;
  const InkStrokeInputBatchStub inputs_;
  const InkStrokeInputBatchViewStub inputs_view_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_STROKE_STUB_H_
