// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_STROKE_INPUT_BATCH_VIEW_STUB_H_
#define PDF_INK_STUB_INK_STROKE_INPUT_BATCH_VIEW_STUB_H_

#include "base/memory/raw_ref.h"
#include "pdf/ink/ink_stroke_input_batch_view.h"

namespace chrome_pdf {

class InkStrokeInputBatchStub;

class InkStrokeInputBatchViewStub : public InkStrokeInputBatchView {
 public:
  explicit InkStrokeInputBatchViewStub(const InkStrokeInputBatchStub& impl);
  InkStrokeInputBatchViewStub(const InkStrokeInputBatchViewStub&) = delete;
  InkStrokeInputBatchViewStub& operator=(const InkStrokeInputBatchViewStub&) =
      delete;
  ~InkStrokeInputBatchViewStub() override;

  // InkStrokeInputBatchView:
  size_t Size() const override;
  InkStrokeInput Get(size_t i) const override;

 private:
  const base::raw_ref<const InkStrokeInputBatchStub> impl_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_STROKE_INPUT_BATCH_VIEW_STUB_H_
