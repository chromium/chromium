// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/position_area.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"

namespace blink {

namespace {

inline PhysicalAxes PhysicalAxisFromRegion(
    PositionAreaRegion region,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  switch (region) {
    case PositionAreaRegion::kTop:
    case PositionAreaRegion::kBottom:
    case PositionAreaRegion::kYStart:
    case PositionAreaRegion::kYEnd:
    case PositionAreaRegion::kYSelfStart:
    case PositionAreaRegion::kYSelfEnd:
      return kPhysicalAxesVertical;
    case PositionAreaRegion::kLeft:
    case PositionAreaRegion::kRight:
    case PositionAreaRegion::kXStart:
    case PositionAreaRegion::kXEnd:
    case PositionAreaRegion::kXSelfStart:
    case PositionAreaRegion::kXSelfEnd:
      return kPhysicalAxesHorizontal;
    case PositionAreaRegion::kInlineStart:
    case PositionAreaRegion::kInlineEnd:
      return container_writing_direction.IsHorizontal()
                 ? kPhysicalAxesHorizontal
                 : kPhysicalAxesVertical;
    case PositionAreaRegion::kSelfInlineStart:
    case PositionAreaRegion::kSelfInlineEnd:
      return self_writing_direction.IsHorizontal() ? kPhysicalAxesHorizontal
                                                   : kPhysicalAxesVertical;
    case PositionAreaRegion::kBlockStart:
    case PositionAreaRegion::kBlockEnd:
      return container_writing_direction.IsHorizontal()
                 ? kPhysicalAxesVertical
                 : kPhysicalAxesHorizontal;
    case PositionAreaRegion::kSelfBlockStart:
    case PositionAreaRegion::kSelfBlockEnd:
      return self_writing_direction.IsHorizontal() ? kPhysicalAxesVertical
                                                   : kPhysicalAxesHorizontal;
    default:
      // Neutral region. Axis depends on the other span or order of appearance
      // if both spans are neutral.
      return kPhysicalAxesNone;
  }
}

// Return the physical axis for an position-area span if given by the regions, or
// kPhysicalAxesNone if we need the direction/writing-mode to decide.
inline PhysicalAxes PhysicalAxisFromSpan(
    PositionAreaRegion start,
    PositionAreaRegion end,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  if (start == PositionAreaRegion::kAll) {
    return kPhysicalAxesNone;
  }
  PositionAreaRegion indicator = start == PositionAreaRegion::kCenter ? end : start;
  return PhysicalAxisFromRegion(indicator, container_writing_direction,
                                self_writing_direction);
}

// Convert a logical region to the corresponding physical region based on the
// span's axis and the direction/writing-mode of the anchored element and its
// containing block.
PositionAreaRegion ToPhysicalRegion(
    PositionAreaRegion region,
    PhysicalAxes axis,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  bool is_horizontal = axis == kPhysicalAxesHorizontal;
  PositionAreaRegion axis_region = region;
  switch (region) {
    case PositionAreaRegion::kNone:
    case PositionAreaRegion::kAll:
      NOTREACHED_IN_MIGRATION()
          << "Should be handled directly in PositionArea::ToPhysical";
      [[fallthrough]];
    case PositionAreaRegion::kCenter:
    case PositionAreaRegion::kTop:
    case PositionAreaRegion::kBottom:
    case PositionAreaRegion::kLeft:
    case PositionAreaRegion::kRight:
      return region;
    case PositionAreaRegion::kStart:
    case PositionAreaRegion::kInlineStart:
    case PositionAreaRegion::kBlockStart:
      axis_region =
          is_horizontal ? PositionAreaRegion::kXStart : PositionAreaRegion::kYStart;
      break;
    case PositionAreaRegion::kEnd:
    case PositionAreaRegion::kInlineEnd:
    case PositionAreaRegion::kBlockEnd:
      axis_region =
          is_horizontal ? PositionAreaRegion::kXEnd : PositionAreaRegion::kYEnd;
      break;
    case PositionAreaRegion::kSelfStart:
    case PositionAreaRegion::kSelfInlineStart:
    case PositionAreaRegion::kSelfBlockStart:
      axis_region = is_horizontal ? PositionAreaRegion::kXSelfStart
                                  : PositionAreaRegion::kYSelfStart;
      break;
    case PositionAreaRegion::kSelfEnd:
    case PositionAreaRegion::kSelfInlineEnd:
    case PositionAreaRegion::kSelfBlockEnd:
      axis_region = is_horizontal ? PositionAreaRegion::kXSelfEnd
                                  : PositionAreaRegion::kYSelfEnd;
      break;
    default:
      break;
  }

  if (is_horizontal) {
    if ((axis_region == PositionAreaRegion::kXStart &&
         container_writing_direction.IsFlippedX()) ||
        (axis_region == PositionAreaRegion::kXEnd &&
         !container_writing_direction.IsFlippedX()) ||
        (axis_region == PositionAreaRegion::kXSelfStart &&
         self_writing_direction.IsFlippedX()) ||
        (axis_region == PositionAreaRegion::kXSelfEnd &&
         !self_writing_direction.IsFlippedX())) {
      return PositionAreaRegion::kRight;
    }
    return PositionAreaRegion::kLeft;
  }

  if ((axis_region == PositionAreaRegion::kYStart &&
       container_writing_direction.IsFlippedY()) ||
      (axis_region == PositionAreaRegion::kYEnd &&
       !container_writing_direction.IsFlippedY()) ||
      (axis_region == PositionAreaRegion::kYSelfStart &&
       self_writing_direction.IsFlippedY()) ||
      (axis_region == PositionAreaRegion::kYSelfEnd &&
       !self_writing_direction.IsFlippedY())) {
    return PositionAreaRegion::kBottom;
  }
  return PositionAreaRegion::kTop;
}

}  // namespace

PositionArea PositionArea::ToPhysical(
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) const {
  if (IsNone()) {
    return *this;
  }
  PhysicalAxes first_axis =
      PhysicalAxisFromSpan(FirstStart(), FirstEnd(),
                           container_writing_direction, self_writing_direction);
  PhysicalAxes second_axis =
      PhysicalAxisFromSpan(SecondStart(), SecondEnd(),
                           container_writing_direction, self_writing_direction);

  if (first_axis == second_axis) {
    CHECK_EQ(first_axis, kPhysicalAxesNone)
        << "Both regions representing the same axis should not happen";
    // If neither span includes a physical keyword, the first refers to the
    // block axis of the containing block, and the second to the inline axis.
    first_axis = ToPhysicalAxes(kLogicalAxesBlock,
                                container_writing_direction.GetWritingMode());
    second_axis = ToPhysicalAxes(kLogicalAxesInline,
                                 container_writing_direction.GetWritingMode());
  } else {
    if (first_axis == kPhysicalAxesNone) {
      first_axis = second_axis ^ kPhysicalAxesBoth;
    } else if (second_axis == kPhysicalAxesNone) {
      second_axis = first_axis ^ kPhysicalAxesBoth;
    }
  }
  DCHECK_EQ(first_axis ^ second_axis, kPhysicalAxesBoth)
      << "Both axes should be defined and orthogonal";

  auto regions = std::to_array<PositionAreaRegion>(
      {PositionAreaRegion::kTop, PositionAreaRegion::kBottom,
       PositionAreaRegion::kLeft, PositionAreaRegion::kRight});

  // Adjust the index to always make the first span the vertical one in the
  // resulting PositionArea, regardless of the original ordering.
  size_t index = first_axis == kPhysicalAxesHorizontal ? 2 : 0;
  if (FirstStart() != PositionAreaRegion::kAll) {
    regions[index] =
        ToPhysicalRegion(FirstStart(), first_axis, container_writing_direction,
                         self_writing_direction);
    regions[index + 1] =
        ToPhysicalRegion(FirstEnd(), first_axis, container_writing_direction,
                         self_writing_direction);
  }
  index = (index + 2) % 4;
  if (SecondStart() != PositionAreaRegion::kAll) {
    regions[index] =
        ToPhysicalRegion(SecondStart(), second_axis,
                         container_writing_direction, self_writing_direction);
    regions[index + 1] =
        ToPhysicalRegion(SecondEnd(), second_axis, container_writing_direction,
                         self_writing_direction);
  }
  if (regions[0] == PositionAreaRegion::kBottom ||
      regions[1] == PositionAreaRegion::kTop) {
    std::swap(regions[0], regions[1]);
  }
  if (regions[2] == PositionAreaRegion::kRight ||
      regions[3] == PositionAreaRegion::kLeft) {
    std::swap(regions[2], regions[3]);
  }
  return PositionArea(regions[0], regions[1], regions[2], regions[3]);
}

std::optional<AnchorQuery> PositionArea::UsedTop() const {
  switch (FirstStart()) {
    case PositionAreaRegion::kTop:
      return std::nullopt;
    case PositionAreaRegion::kCenter:
      return AnchorTop();
    case PositionAreaRegion::kBottom:
      return AnchorBottom();
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PositionAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> PositionArea::UsedBottom() const {
  switch (FirstEnd()) {
    case PositionAreaRegion::kTop:
      return AnchorTop();
    case PositionAreaRegion::kCenter:
      return AnchorBottom();
    case PositionAreaRegion::kBottom:
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PositionAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> PositionArea::UsedLeft() const {
  switch (SecondStart()) {
    case PositionAreaRegion::kLeft:
      return std::nullopt;
    case PositionAreaRegion::kCenter:
      return AnchorLeft();
    case PositionAreaRegion::kRight:
      return AnchorRight();
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PositionAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> PositionArea::UsedRight() const {
  switch (SecondEnd()) {
    case PositionAreaRegion::kLeft:
      return AnchorLeft();
    case PositionAreaRegion::kCenter:
      return AnchorRight();
    case PositionAreaRegion::kRight:
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PositionAreaRegion::kNone:
      return std::nullopt;
  }
}

std::pair<StyleSelfAlignmentData, StyleSelfAlignmentData>
PositionArea::AlignJustifySelfFromPhysical(
    WritingDirectionMode container_writing_direction,
    bool is_containing_block_scrollable) const {
  const OverflowAlignment overflow = is_containing_block_scrollable
                                         ? OverflowAlignment::kUnsafe
                                         : OverflowAlignment::kDefault;

  StyleSelfAlignmentData align(ItemPosition::kStart, overflow);
  StyleSelfAlignmentData align_reverse(ItemPosition::kEnd, overflow);
  StyleSelfAlignmentData justify(ItemPosition::kStart, overflow);
  StyleSelfAlignmentData justify_reverse(ItemPosition::kEnd, overflow);

  if ((FirstStart() == PositionAreaRegion::kTop &&
       FirstEnd() == PositionAreaRegion::kBottom) ||
      (FirstStart() == PositionAreaRegion::kCenter &&
       FirstEnd() == PositionAreaRegion::kCenter)) {
    // 'center' or 'all' should align with anchor center.
    align = align_reverse = {ItemPosition::kAnchorCenter, overflow};
  } else {
    // 'top' and 'top center' aligns with end, 'bottom' and 'center bottom' with
    // start.
    if (FirstStart() == PositionAreaRegion::kTop) {
      std::swap(align, align_reverse);
    }
  }
  if ((SecondStart() == PositionAreaRegion::kLeft &&
       SecondEnd() == PositionAreaRegion::kRight) ||
      (SecondStart() == PositionAreaRegion::kCenter &&
       SecondEnd() == PositionAreaRegion::kCenter)) {
    // 'center' or 'all' should align with anchor center.
    justify = justify_reverse = {ItemPosition::kAnchorCenter, overflow};
  } else {
    // 'left' and 'left center' aligns with end, 'right' and 'center right' with
    // start.
    if (SecondStart() == PositionAreaRegion::kLeft) {
      std::swap(justify, justify_reverse);
    }
  }

  if ((FirstStart() == PositionAreaRegion::kTop &&
       FirstEnd() == PositionAreaRegion::kTop) ||
      (FirstStart() == PositionAreaRegion::kBottom &&
       FirstEnd() == PositionAreaRegion::kBottom)) {
    align.SetOverflow(OverflowAlignment::kUnsafe);
    align_reverse.SetOverflow(OverflowAlignment::kUnsafe);
  }
  if ((SecondStart() == PositionAreaRegion::kLeft &&
       SecondEnd() == PositionAreaRegion::kLeft) ||
      (SecondStart() == PositionAreaRegion::kRight &&
       SecondEnd() == PositionAreaRegion::kRight)) {
    justify.SetOverflow(OverflowAlignment::kUnsafe);
    justify_reverse.SetOverflow(OverflowAlignment::kUnsafe);
  }

  PhysicalToLogical converter(container_writing_direction, align,
                              justify_reverse, align_reverse, justify);
  return {converter.BlockStart(), converter.InlineStart()};
}

AnchorQuery PositionArea::AnchorTop() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kTop);
}

AnchorQuery PositionArea::AnchorBottom() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kBottom);
}

AnchorQuery PositionArea::AnchorLeft() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kLeft);
}

AnchorQuery PositionArea::AnchorRight() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kRight);
}

}  // namespace blink
