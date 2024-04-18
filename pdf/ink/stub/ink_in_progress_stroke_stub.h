// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_STUB_INK_IN_PROGRESS_STROKE_STUB_H_
#define PDF_INK_STUB_INK_IN_PROGRESS_STROKE_STUB_H_

#include "pdf/ink/ink_in_progress_stroke.h"

namespace chrome_pdf {

class InkInProgressStrokeStub : public InkInProgressStroke {
 public:
  ~InkInProgressStrokeStub() override;

  // InkInProgressStroke:
  void Start(const InkBrush& brush) override;
  bool EnqueueInputs(const InkStrokeInputBatch* real_inputs,
                     const InkStrokeInputBatch* predicted_inputs) override;
  void FinishInputs() override;
  bool UpdateShape(float current_elapsed_time_seconds) override;
  std::unique_ptr<InkStroke> CopyToStroke() const override;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_STUB_INK_IN_PROGRESS_STROKE_STUB_H_
