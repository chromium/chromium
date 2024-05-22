// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_modeled_shape_stub.h"

namespace chrome_pdf {

InkModeledShapeStub::InkModeledShapeStub() = default;

InkModeledShapeStub::~InkModeledShapeStub() = default;

uint32_t InkModeledShapeStub::RenderGroupCount() const {
  return 0;
}

std::vector<InkModeledShape::Outline> InkModeledShapeStub::GetOutlines(
    uint32_t group_index) const {
  return {};
}

}  // namespace chrome_pdf
