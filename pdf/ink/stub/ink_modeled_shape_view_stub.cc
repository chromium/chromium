// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"

namespace chrome_pdf {

InkModeledShapeViewStub::InkModeledShapeViewStub() = default;

InkModeledShapeViewStub::~InkModeledShapeViewStub() = default;

uint32_t InkModeledShapeViewStub::RenderGroupCount() const {
  return 0;
}

std::vector<InkModeledShapeView::OutlinePositions>
InkModeledShapeViewStub::GetRenderGroupOutlinePositions(
    uint32_t group_index) const {
  return {};
}

InkRect InkModeledShapeViewStub::Bounds() const {
  return {};
}

}  // namespace chrome_pdf
