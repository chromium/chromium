// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_in_progress_stroke_stub.h"

#include <memory>

#include "pdf/ink/ink_brush.h"
#include "pdf/ink/stub/ink_stroke_stub.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkInProgressStroke> InkInProgressStroke::Create() {
  return std::make_unique<InkInProgressStrokeStub>();
}

InkInProgressStrokeStub::InkInProgressStrokeStub() = default;

InkInProgressStrokeStub::~InkInProgressStrokeStub() = default;

void InkInProgressStrokeStub::Start(const InkBrush& brush) {
  brush_color_ = brush.GetColor();
}

bool InkInProgressStrokeStub::EnqueueInputs(
    const InkStrokeInputBatch* real_inputs,
    const InkStrokeInputBatch* predicted_inputs) {
  if (!real_inputs) {
    return false;
  }

  // Capture copy of input.
  inputs_ = *static_cast<const InkStrokeInputBatchStub*>(real_inputs);
  return true;
}

void InkInProgressStrokeStub::FinishInputs() {}

bool InkInProgressStrokeStub::UpdateShape(float current_elapsed_time_seconds) {
  // Pretend shape update succeeded, even though nothing is done here.
  return true;
}

std::unique_ptr<InkStroke> InkInProgressStrokeStub::CopyToStroke() const {
  return std::make_unique<InkStrokeStub>(brush_color_, inputs_);
}

}  // namespace chrome_pdf
