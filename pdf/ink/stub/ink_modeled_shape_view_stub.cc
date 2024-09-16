// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink/stub/ink_modeled_shape_view_stub.h"

#include <memory>

#include "base/check_op.h"

namespace chrome_pdf {

InkModeledShapeViewStub::InkModeledShapeViewStub() = default;

InkModeledShapeViewStub::InkModeledShapeViewStub(
    const GroupsOutlines& groups_outlines,
    const InkRect& bounds)
    : groups_outlines_(groups_outlines), bounds_(bounds) {}

InkModeledShapeViewStub::~InkModeledShapeViewStub() = default;

uint32_t InkModeledShapeViewStub::RenderGroupCount() const {
  return groups_outlines_.size();
}

uint32_t InkModeledShapeViewStub::OutlineCount(uint32_t group_index) const {
  CHECK_LT(group_index, groups_outlines_.size());
  return groups_outlines_[group_index].size();
}

std::vector<InkModeledShapeView::OutlinePositions>
InkModeledShapeViewStub::GetRenderGroupOutlinePositions(
    uint32_t group_index) const {
  CHECK_LT(group_index, groups_outlines_.size());
  return groups_outlines_[group_index];
}

InkRect InkModeledShapeViewStub::Bounds() const {
  return bounds_;
}

}  // namespace chrome_pdf
