// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"

#include <memory>

#include "base/check_op.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkStrokeInputBatch> InkStrokeInputBatch::Create(
    const std::vector<InkStrokeInput>& inputs) {
  return std::make_unique<InkStrokeInputBatchStub>(inputs);
}

InkStrokeInputBatchStub::InkStrokeInputBatchStub() = default;

InkStrokeInputBatchStub::InkStrokeInputBatchStub(
    const InkStrokeInputBatchStub& other) = default;

InkStrokeInputBatchStub& InkStrokeInputBatchStub::operator=(
    const InkStrokeInputBatchStub& other) = default;

InkStrokeInputBatchStub::InkStrokeInputBatchStub(
    const std::vector<InkStrokeInput>& inputs)
    : inputs_(std::move(inputs)) {}

InkStrokeInputBatchStub::~InkStrokeInputBatchStub() = default;

size_t InkStrokeInputBatchStub::Size() const {
  return inputs_.size();
}

InkStrokeInput InkStrokeInputBatchStub::Get(size_t i) const {
  CHECK_LT(i, inputs_.size());
  return inputs_[i];
}

}  // namespace chrome_pdf
