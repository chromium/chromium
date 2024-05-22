// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_stroke_stub.h"

#include <memory>

#include "pdf/ink/stub/ink_modeled_shape_stub.h"

namespace chrome_pdf {

InkStrokeStub::InkStrokeStub()
    : shape_(std::make_unique<InkModeledShapeStub>()) {}

InkStrokeStub::~InkStrokeStub() = default;

const InkModeledShape* InkStrokeStub::GetShape() const {
  return shape_.get();
}

}  // namespace chrome_pdf
