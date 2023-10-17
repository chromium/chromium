// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_STATIC_POSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_STATIC_POSITION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

struct PhysicalStaticPosition;

// Represents the static-position of an OOF-positioned descendant, in the
// logical coordinate space.
//
// |offset| is the position of the descandant's |inline_edge|, and |block_edge|.
struct CORE_EXPORT LogicalStaticPosition {
  enum InlineEdge { kInlineStart, kInlineCenter, kInlineEnd };
  enum BlockEdge { kBlockStart, kBlockCenter, kBlockEnd };

  inline PhysicalStaticPosition ConvertToPhysical(
      const WritingModeConverter& converter) const;

  LogicalOffset offset;
  InlineEdge inline_edge;
  BlockEdge block_edge;
};

// Similar to |LogicalStaticPosition| but in the physical coordinate space.
struct CORE_EXPORT PhysicalStaticPosition {
  enum HorizontalEdge { kLeft, kHorizontalCenter, kRight };
  enum VerticalEdge { kTop, kVerticalCenter, kBottom };

  PhysicalOffset offset;
  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;

  LogicalStaticPosition ConvertToLogical(
      const WritingModeConverter& converter) const {
    LogicalOffset logical_offset =
        converter.ToLogical(offset, /* inner_size */ PhysicalSize());

    using InlineEdge = LogicalStaticPosition::InlineEdge;
    using BlockEdge = LogicalStaticPosition::BlockEdge;

    InlineEdge inline_edge;
    BlockEdge block_edge;

    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        inline_edge = ((horizontal_edge == kLeft) == converter.IsLtr())
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (vertical_edge == kTop) ? BlockEdge::kBlockStart
                                             : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        inline_edge = ((vertical_edge == kTop) == converter.IsLtr())
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kRight) ? BlockEdge::kBlockStart
                                                 : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalLr:
        inline_edge = ((vertical_edge == kTop) == converter.IsLtr())
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kSidewaysLr:
        inline_edge = ((vertical_edge == kBottom) == converter.IsLtr())
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                : BlockEdge::kBlockEnd;
        break;
    }

    // Adjust for uncommon "center" static-positions.
    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        inline_edge = (horizontal_edge == kHorizontalCenter)
                          ? InlineEdge::kInlineCenter
                          : inline_edge;
        block_edge = (vertical_edge == kVerticalCenter)
                         ? BlockEdge::kBlockCenter
                         : block_edge;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        inline_edge = (vertical_edge == kVerticalCenter)
                          ? InlineEdge::kInlineCenter
                          : inline_edge;
        block_edge = (horizontal_edge == kHorizontalCenter)
                         ? BlockEdge::kBlockCenter
                         : block_edge;
        break;
    }

    return {logical_offset, inline_edge, block_edge};
  }
};

inline PhysicalStaticPosition LogicalStaticPosition::ConvertToPhysical(
    const WritingModeConverter& converter) const {
  PhysicalOffset physical_offset =
      converter.ToPhysical(offset, /* inner_size */ PhysicalSize());

  using HorizontalEdge = PhysicalStaticPosition::HorizontalEdge;
  using VerticalEdge = PhysicalStaticPosition::VerticalEdge;

  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;

  switch (converter.GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      horizontal_edge = ((inline_edge == kInlineStart) == converter.IsLtr())
                            ? HorizontalEdge::kLeft
                            : HorizontalEdge::kRight;
      vertical_edge = (block_edge == kBlockStart) ? VerticalEdge::kTop
                                                  : VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      horizontal_edge = (block_edge == kBlockEnd) ? HorizontalEdge::kLeft
                                                  : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == converter.IsLtr())
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalLr:
      horizontal_edge = (block_edge == kBlockStart) ? HorizontalEdge::kLeft
                                                    : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == converter.IsLtr())
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
    case WritingMode::kSidewaysLr:
      horizontal_edge = (block_edge == kBlockStart) ? HorizontalEdge::kLeft
                                                    : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineEnd) == converter.IsLtr())
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
  }

  // Adjust for uncommon "center" static-positions.
  switch (converter.GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      horizontal_edge = (inline_edge == kInlineCenter)
                            ? HorizontalEdge::kHorizontalCenter
                            : horizontal_edge;
      vertical_edge = (block_edge == kBlockCenter)
                          ? VerticalEdge::kVerticalCenter
                          : vertical_edge;
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysLr:
      horizontal_edge = (block_edge == kBlockCenter)
                            ? HorizontalEdge::kHorizontalCenter
                            : horizontal_edge;
      vertical_edge = (inline_edge == kInlineCenter)
                          ? VerticalEdge::kVerticalCenter
                          : vertical_edge;
      break;
  }

  return {physical_offset, horizontal_edge, vertical_edge};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_STATIC_POSITION_H_
