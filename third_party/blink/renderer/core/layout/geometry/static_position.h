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
// `offset` is the position of the descandant's `inline_edge`, and `block_edge`.
//
// `safe_inline_edge`, and `safe_block_edge` keep track of what edge would be
// considered safe if the overflow alignment is 'safe' [1]. This should always
// begin at the start but will be adjusted based on parent chain conversion.
//
// [1] https://drafts.csswg.org/css-align/#valdef-overflow-position-safe
struct CORE_EXPORT LogicalStaticPosition {
  enum InlineEdge { kInlineStart, kInlineCenter, kInlineEnd };
  enum BlockEdge { kBlockStart, kBlockCenter, kBlockEnd };

  inline PhysicalStaticPosition ConvertToPhysical(
      const WritingModeConverter& converter) const;

  LogicalOffset offset;
  InlineEdge inline_edge;
  BlockEdge block_edge;
  InlineEdge safe_inline_edge;
  BlockEdge safe_block_edge;
};

// Similar to `LogicalStaticPosition` but in the physical coordinate space.
struct CORE_EXPORT PhysicalStaticPosition {
  enum HorizontalEdge { kLeft, kHorizontalCenter, kRight };
  enum VerticalEdge { kTop, kVerticalCenter, kBottom };

  PhysicalOffset offset;
  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;
  HorizontalEdge safe_horizontal_edge;
  VerticalEdge safe_vertical_edge;

  LogicalStaticPosition ConvertToLogical(
      const WritingModeConverter& converter) const;
};

inline PhysicalStaticPosition LogicalStaticPosition::ConvertToPhysical(
    const WritingModeConverter& converter) const {
  PhysicalOffset physical_offset =
      converter.ToPhysical(offset, /*inner_size=*/PhysicalSize());

  using HorizontalEdge = PhysicalStaticPosition::HorizontalEdge;
  using VerticalEdge = PhysicalStaticPosition::VerticalEdge;

  auto ConvertEdgesToPhysical = [&](const WritingModeConverter& converter,
                                    const InlineEdge inline_edge,
                                    const BlockEdge block_edge,
                                    HorizontalEdge& horizontal_edge_out,
                                    VerticalEdge& vertical_edge_out) {
    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        horizontal_edge_out =
            ((inline_edge == kInlineStart) == converter.IsLtr())
                ? HorizontalEdge::kLeft
                : HorizontalEdge::kRight;
        vertical_edge_out = (block_edge == kBlockStart) ? VerticalEdge::kTop
                                                        : VerticalEdge::kBottom;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        horizontal_edge_out = (block_edge == kBlockEnd)
                                  ? HorizontalEdge::kLeft
                                  : HorizontalEdge::kRight;
        vertical_edge_out = ((inline_edge == kInlineStart) == converter.IsLtr())
                                ? VerticalEdge::kTop
                                : VerticalEdge::kBottom;
        break;
      case WritingMode::kVerticalLr:
        horizontal_edge_out = (block_edge == kBlockStart)
                                  ? HorizontalEdge::kLeft
                                  : HorizontalEdge::kRight;
        vertical_edge_out = ((inline_edge == kInlineStart) == converter.IsLtr())
                                ? VerticalEdge::kTop
                                : VerticalEdge::kBottom;
        break;
      case WritingMode::kSidewaysLr:
        horizontal_edge_out = (block_edge == kBlockStart)
                                  ? HorizontalEdge::kLeft
                                  : HorizontalEdge::kRight;
        vertical_edge_out = ((inline_edge == kInlineEnd) == converter.IsLtr())
                                ? VerticalEdge::kTop
                                : VerticalEdge::kBottom;
        break;
    }

    // Adjust for uncommon "center" static-positions.
    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        horizontal_edge_out = (inline_edge == kInlineCenter)
                                  ? HorizontalEdge::kHorizontalCenter
                                  : horizontal_edge_out;
        vertical_edge_out = (block_edge == kBlockCenter)
                                ? VerticalEdge::kVerticalCenter
                                : vertical_edge_out;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        horizontal_edge_out = (block_edge == kBlockCenter)
                                  ? HorizontalEdge::kHorizontalCenter
                                  : horizontal_edge_out;
        vertical_edge_out = (inline_edge == kInlineCenter)
                                ? VerticalEdge::kVerticalCenter
                                : vertical_edge_out;
        break;
    }
  };

  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;
  HorizontalEdge safe_horizontal_edge;
  VerticalEdge safe_vertical_edge;

  ConvertEdgesToPhysical(converter, inline_edge, block_edge, horizontal_edge,
                         vertical_edge);
  ConvertEdgesToPhysical(converter, safe_inline_edge, safe_block_edge,
                         safe_horizontal_edge, safe_vertical_edge);

  return {physical_offset, horizontal_edge, vertical_edge, safe_horizontal_edge,
          safe_vertical_edge};
}

inline LogicalStaticPosition PhysicalStaticPosition::ConvertToLogical(
    const WritingModeConverter& converter) const {
  LogicalOffset logical_offset =
      converter.ToLogical(offset, /*inner_size=*/PhysicalSize());

  using InlineEdge = LogicalStaticPosition::InlineEdge;
  using BlockEdge = LogicalStaticPosition::BlockEdge;

  auto ConvertEdgesToLogical = [&](const WritingModeConverter& converter,
                                   const HorizontalEdge horizontal_edge,
                                   const VerticalEdge vertical_edge,
                                   InlineEdge& inline_edge_out,
                                   BlockEdge& block_edge_out) {
    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        inline_edge_out = ((horizontal_edge == kLeft) == converter.IsLtr())
                              ? InlineEdge::kInlineStart
                              : InlineEdge::kInlineEnd;
        block_edge_out = (vertical_edge == kTop) ? BlockEdge::kBlockStart
                                                 : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        inline_edge_out = ((vertical_edge == kTop) == converter.IsLtr())
                              ? InlineEdge::kInlineStart
                              : InlineEdge::kInlineEnd;
        block_edge_out = (horizontal_edge == kRight) ? BlockEdge::kBlockStart
                                                     : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalLr:
        inline_edge_out = ((vertical_edge == kTop) == converter.IsLtr())
                              ? InlineEdge::kInlineStart
                              : InlineEdge::kInlineEnd;
        block_edge_out = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                    : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kSidewaysLr:
        inline_edge_out = ((vertical_edge == kBottom) == converter.IsLtr())
                              ? InlineEdge::kInlineStart
                              : InlineEdge::kInlineEnd;
        block_edge_out = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                    : BlockEdge::kBlockEnd;
        break;
    }

    // Adjust for uncommon "center" static-positions.
    switch (converter.GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        inline_edge_out = (horizontal_edge == kHorizontalCenter)
                              ? InlineEdge::kInlineCenter
                              : inline_edge_out;
        block_edge_out = (vertical_edge == kVerticalCenter)
                             ? BlockEdge::kBlockCenter
                             : block_edge_out;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        inline_edge_out = (vertical_edge == kVerticalCenter)
                              ? InlineEdge::kInlineCenter
                              : inline_edge_out;
        block_edge_out = (horizontal_edge == kHorizontalCenter)
                             ? BlockEdge::kBlockCenter
                             : block_edge_out;
        break;
    }
  };

  InlineEdge inline_edge;
  BlockEdge block_edge;
  InlineEdge safe_inline_edge;
  BlockEdge safe_block_edge;

  ConvertEdgesToLogical(converter, horizontal_edge, vertical_edge, inline_edge,
                        block_edge);
  ConvertEdgesToLogical(converter, safe_horizontal_edge, safe_vertical_edge,
                        safe_inline_edge, safe_block_edge);

  return {logical_offset, inline_edge, block_edge, safe_inline_edge,
          safe_block_edge};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_STATIC_POSITION_H_
