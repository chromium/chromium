// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"

#include <memory>

namespace chrome_pdf {

// static
std::unique_ptr<InkStrokeInputBatch> InkStrokeInputBatch::Create(
    const std::vector<InkStrokeInput>& inputs) {
  return std::make_unique<InkStrokeInputBatchStub>();
}

InkStrokeInputBatchStub::InkStrokeInputBatchStub() = default;

InkStrokeInputBatchStub::~InkStrokeInputBatchStub() = default;

}  // namespace chrome_pdf
