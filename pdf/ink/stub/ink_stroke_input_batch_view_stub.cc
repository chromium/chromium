// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_input_batch_view_stub.h"

#include "pdf/ink/stub/ink_stroke_input_batch_stub.h"

namespace chrome_pdf {

InkStrokeInputBatchViewStub::InkStrokeInputBatchViewStub(
    const InkStrokeInputBatchStub& impl)
    : impl_(impl) {}

InkStrokeInputBatchViewStub::~InkStrokeInputBatchViewStub() = default;

size_t InkStrokeInputBatchViewStub::Size() const {
  return impl_->Size();
}

InkStrokeInput InkStrokeInputBatchViewStub::Get(size_t i) const {
  return impl_->Get(i);
}

}  // namespace chrome_pdf
