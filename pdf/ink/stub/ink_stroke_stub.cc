// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_stub.h"

#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"
#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"

namespace chrome_pdf {

InkStrokeStub::InkStrokeStub(const InkStrokeInputBatchStub& inputs)
    : inputs_(inputs) {}

InkStrokeStub::~InkStrokeStub() = default;

const InkStrokeInputBatch& InkStrokeStub::GetInputs() const {
  return inputs_;
}

const InkModeledShapeView& InkStrokeStub::GetShape() const {
  return shape_;
}

}  // namespace chrome_pdf
