// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"

namespace blink {

LayoutOutsideListMarker::LayoutOutsideListMarker(Element* element)
    : LayoutBlockFlow(element) {}

LayoutOutsideListMarker::~LayoutOutsideListMarker() = default;

bool LayoutOutsideListMarker::IsMarkerImage() const {
  NOT_DESTROYED();
  return list_marker_.IsMarkerImage(*this);
}

void LayoutOutsideListMarker::UpdateLayout() {
  NOT_DESTROYED();
  LayoutBlockFlow::UpdateLayout();

  LayoutUnit block_offset = LogicalTop();
  const LayoutBlockFlow* list_item = list_marker_.ListItemBlockFlow(*this);
  for (LayoutBox* o = ParentBox(); o && o != list_item; o = o->ParentBox()) {
    block_offset += o->LogicalTop();
  }
  if (list_item->StyleRef().IsLeftToRightDirection()) {
    list_item_inline_start_offset_ = list_item->LogicalLeftOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  } else {
    list_item_inline_start_offset_ = list_item->LogicalRightOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  }
  UpdateMargins();
}

void LayoutOutsideListMarker::UpdateMargins() {
  NOT_DESTROYED();
  auto [margin_start, margin_end] = ListMarker::InlineMarginsForOutside(
      GetDocument(), StyleRef(), list_marker_.ListItem(*this)->StyleRef(),
      PreferredLogicalWidths().min_size);
  SetMarginStart(margin_start);
  SetMarginEnd(margin_end);
}

LayoutUnit LayoutOutsideListMarker::LineHeight(
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  if (line_position_mode == kPositionOfInteriorLineBoxes) {
    return list_marker_.ListItemBlockFlow(*this)->LineHeight(
        first_line, direction, line_position_mode);
  }
  return LayoutBlockFlow::LineHeight(first_line, direction, line_position_mode);
}

LayoutUnit LayoutOutsideListMarker::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  if (line_position_mode == kPositionOfInteriorLineBoxes) {
    return list_marker_.ListItemBlockFlow(*this)->BaselinePosition(
        baseline_type, first_line, direction, line_position_mode);
  }
  return LayoutBlockFlow::BaselinePosition(baseline_type, first_line, direction,
                                           line_position_mode);
}

}  // namespace blink
