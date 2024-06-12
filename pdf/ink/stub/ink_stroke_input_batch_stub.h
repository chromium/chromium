// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_STROKE_INPUT_BATCH_STUB_H_
#define PDF_INK_STUB_INK_STROKE_INPUT_BATCH_STUB_H_

#include <vector>

#include "pdf/ink/ink_stroke_input.h"
#include "pdf/ink/ink_stroke_input_batch.h"

namespace chrome_pdf {

class InkStrokeInputBatchStub : public InkStrokeInputBatch {
 public:
  InkStrokeInputBatchStub();
  explicit InkStrokeInputBatchStub(const std::vector<InkStrokeInput>& inputs);
  InkStrokeInputBatchStub(const InkStrokeInputBatchStub& other);
  InkStrokeInputBatchStub& operator=(const InkStrokeInputBatchStub& other);
  ~InkStrokeInputBatchStub() override;

  // `InkStrokeInputBatch`:
  size_t Size() const override;
  InkStrokeInput Get(size_t i) const override;

 private:
  std::vector<InkStrokeInput> inputs_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_STROKE_INPUT_BATCH_STUB_H_
