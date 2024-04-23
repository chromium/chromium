// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_in_progress_stroke_stub.h"

#include <memory>

#include "pdf/ink/ink_stroke.h"

namespace chrome_pdf {

// static
std::unique_ptr<InkInProgressStroke> InkInProgressStroke::Create() {
  return nullptr;
}

InkInProgressStrokeStub::~InkInProgressStrokeStub() = default;

void InkInProgressStrokeStub::Start(const InkBrush& brush) {}

bool InkInProgressStrokeStub::EnqueueInputs(
    const InkStrokeInputBatch* real_inputs,
    const InkStrokeInputBatch* predicted_inputs) {
  return false;
}

void InkInProgressStrokeStub::FinishInputs() {}

bool InkInProgressStrokeStub::UpdateShape(float current_elapsed_time_seconds) {
  return false;
}

std::unique_ptr<InkStroke> InkInProgressStrokeStub::CopyToStroke() const {
  return nullptr;
}

}  // namespace chrome_pdf
