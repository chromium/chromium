// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_STROKE_STUB_H_
#define PDF_INK_STUB_INK_STROKE_STUB_H_

#include <memory>

#include "pdf/ink/ink_stroke.h"
#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"

namespace chrome_pdf {

class InkModeledShape;
class InkModeledShapeStub;

class InkStrokeStub : public InkStroke {
 public:
  explicit InkStrokeStub(const InkStrokeInputBatchStub& inputs);
  ~InkStrokeStub() override;

  // InkStroke:
  const InkStrokeInputBatch& GetInputs() const override;
  const InkModeledShape* GetShape() const override;

 private:
  const std::unique_ptr<InkModeledShapeStub> shape_;
  const InkStrokeInputBatchStub inputs_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_STROKE_STUB_H_
