// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_in_progress_stroke_stub.h"

#include <memory>

#include "pdf/ink/stub/ink_stroke_stub.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkInProgressStroke> InkInProgressStroke::Create() {
  return std::make_unique<InkInProgressStrokeStub>();
}

InkInProgressStrokeStub::~InkInProgressStrokeStub() = default;

void InkInProgressStrokeStub::Start(const InkBrush& brush) {}

bool InkInProgressStrokeStub::EnqueueInputs(
    const InkStrokeInputBatch* real_inputs,
    const InkStrokeInputBatch* predicted_inputs) {
  // Pretend enqueuing succeeded, even though nothing is done here.
  return true;
}

void InkInProgressStrokeStub::FinishInputs() {}

bool InkInProgressStrokeStub::UpdateShape(float current_elapsed_time_seconds) {
  // Pretend shape update succeeded, even though nothing is done here.
  return true;
}

std::unique_ptr<InkStroke> InkInProgressStrokeStub::CopyToStroke() const {
  return std::make_unique<InkStrokeStub>();
}

}  // namespace chrome_pdf
