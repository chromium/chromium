// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_STROKE_STUB_H_
#define PDF_INK_STUB_INK_STROKE_STUB_H_

#include "pdf/ink/ink_stroke.h"
#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_view_stub.h"

namespace chrome_pdf {

class InkModeledShapeViewStub;

class InkStrokeStub : public InkStroke {
 public:
  explicit InkStrokeStub(const InkStrokeInputBatchStub& inputs);
  InkStrokeStub(const InkStrokeStub&) = delete;
  InkStrokeStub& operator=(const InkStrokeStub&) = delete;
  ~InkStrokeStub() override;

  // InkStroke:
  const InkStrokeInputBatchView& GetInputs() const override;
  const InkModeledShapeView& GetShape() const override;

 private:
  const InkModeledShapeViewStub shape_;
  const InkStrokeInputBatchStub inputs_;
  const InkStrokeInputBatchViewStub inputs_view_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_STROKE_STUB_H_
