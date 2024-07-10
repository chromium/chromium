// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/style/inset_area.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"

namespace blink {

namespace {

inline PhysicalAxes PhysicalAxisFromRegion(
    InsetAreaRegion region,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  switch (region) {
    case InsetAreaRegion::kTop:
    case InsetAreaRegion::kBottom:
    case InsetAreaRegion::kYStart:
    case InsetAreaRegion::kYEnd:
    case InsetAreaRegion::kYSelfStart:
    case InsetAreaRegion::kYSelfEnd:
      return kPhysicalAxesVertical;
    case InsetAreaRegion::kLeft:
    case InsetAreaRegion::kRight:
    case InsetAreaRegion::kXStart:
    case InsetAreaRegion::kXEnd:
    case InsetAreaRegion::kXSelfStart:
    case InsetAreaRegion::kXSelfEnd:
      return kPhysicalAxesHorizontal;
    case InsetAreaRegion::kInlineStart:
    case InsetAreaRegion::kInlineEnd:
      return container_writing_direction.IsHorizontal()
                 ? kPhysicalAxesHorizontal
                 : kPhysicalAxesVertical;
    case InsetAreaRegion::kSelfInlineStart:
    case InsetAreaRegion::kSelfInlineEnd:
      return self_writing_direction.IsHorizontal() ? kPhysicalAxesHorizontal
                                                   : kPhysicalAxesVertical;
    case InsetAreaRegion::kBlockStart:
    case InsetAreaRegion::kBlockEnd:
      return container_writing_direction.IsHorizontal()
                 ? kPhysicalAxesVertical
                 : kPhysicalAxesHorizontal;
    case InsetAreaRegion::kSelfBlockStart:
    case InsetAreaRegion::kSelfBlockEnd:
      return self_writing_direction.IsHorizontal() ? kPhysicalAxesVertical
                                                   : kPhysicalAxesHorizontal;
    default:
      // Neutral region. Axis depends on the other span or order of appearance
      // if both spans are neutral.
      return kPhysicalAxesNone;
  }
}

// Return the physical axis for an inset-area span if given by the regions, or
// kPhysicalAxesNone if we need the direction/writing-mode to decide.
inline PhysicalAxes PhysicalAxisFromSpan(
    InsetAreaRegion start,
    InsetAreaRegion end,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  if (start == InsetAreaRegion::kAll) {
    return kPhysicalAxesNone;
  }
  InsetAreaRegion indicator = start == InsetAreaRegion::kCenter ? end : start;
  return PhysicalAxisFromRegion(indicator, container_writing_direction,
                                self_writing_direction);
}

// Convert a logical region to the corresponding physical region based on the
// span's axis and the direction/writing-mode of the anchored element and its
// containing block.
InsetAreaRegion ToPhysicalRegion(
    InsetAreaRegion region,
    PhysicalAxes axis,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  bool is_horizontal = axis == kPhysicalAxesHorizontal;
  InsetAreaRegion axis_region = region;
  switch (region) {
    case InsetAreaRegion::kNone:
    case InsetAreaRegion::kAll:
      NOTREACHED_IN_MIGRATION()
          << "Should be handled directly in InsetArea::ToPhysical";
      [[fallthrough]];
    case InsetAreaRegion::kCenter:
    case InsetAreaRegion::kTop:
    case InsetAreaRegion::kBottom:
    case InsetAreaRegion::kLeft:
    case InsetAreaRegion::kRight:
      return region;
    case InsetAreaRegion::kStart:
    case InsetAreaRegion::kInlineStart:
    case InsetAreaRegion::kBlockStart:
      axis_region =
          is_horizontal ? InsetAreaRegion::kXStart : InsetAreaRegion::kYStart;
      break;
    case InsetAreaRegion::kEnd:
    case InsetAreaRegion::kInlineEnd:
    case InsetAreaRegion::kBlockEnd:
      axis_region =
          is_horizontal ? InsetAreaRegion::kXEnd : InsetAreaRegion::kYEnd;
      break;
    case InsetAreaRegion::kSelfStart:
    case InsetAreaRegion::kSelfInlineStart:
    case InsetAreaRegion::kSelfBlockStart:
      axis_region = is_horizontal ? InsetAreaRegion::kXSelfStart
                                  : InsetAreaRegion::kYSelfStart;
      break;
    case InsetAreaRegion::kSelfEnd:
    case InsetAreaRegion::kSelfInlineEnd:
    case InsetAreaRegion::kSelfBlockEnd:
      axis_region = is_horizontal ? InsetAreaRegion::kXSelfEnd
                                  : InsetAreaRegion::kYSelfEnd;
      break;
    default:
      break;
  }

  if (is_horizontal) {
    if ((axis_region == InsetAreaRegion::kXStart &&
         container_writing_direction.IsFlippedX()) ||
        (axis_region == InsetAreaRegion::kXEnd &&
         !container_writing_direction.IsFlippedX()) ||
        (axis_region == InsetAreaRegion::kXSelfStart &&
         self_writing_direction.IsFlippedX()) ||
        (axis_region == InsetAreaRegion::kXSelfEnd &&
         !self_writing_direction.IsFlippedX())) {
      return InsetAreaRegion::kRight;
    }
    return InsetAreaRegion::kLeft;
  }

  if ((axis_region == InsetAreaRegion::kYStart &&
       container_writing_direction.IsFlippedY()) ||
      (axis_region == InsetAreaRegion::kYEnd &&
       !container_writing_direction.IsFlippedY()) ||
      (axis_region == InsetAreaRegion::kYSelfStart &&
       self_writing_direction.IsFlippedY()) ||
      (axis_region == InsetAreaRegion::kYSelfEnd &&
       !self_writing_direction.IsFlippedY())) {
    return InsetAreaRegion::kBottom;
  }
  return InsetAreaRegion::kTop;
}

}  // namespace

InsetArea InsetArea::ToPhysical(
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

  InsetAreaRegion regions[4] = {InsetAreaRegion::kTop, InsetAreaRegion::kBottom,
                                InsetAreaRegion::kLeft,
                                InsetAreaRegion::kRight};

  // Adjust the index to always make the first span the vertical one in the
  // resulting InsetArea, regardless of the original ordering.
  size_t index = first_axis == kPhysicalAxesHorizontal ? 2 : 0;
  if (FirstStart() != InsetAreaRegion::kAll) {
    regions[index] =
        ToPhysicalRegion(FirstStart(), first_axis, container_writing_direction,
                         self_writing_direction);
    regions[index + 1] =
        ToPhysicalRegion(FirstEnd(), first_axis, container_writing_direction,
                         self_writing_direction);
  }
  index = (index + 2) % 4;
  if (SecondStart() != InsetAreaRegion::kAll) {
    regions[index] =
        ToPhysicalRegion(SecondStart(), second_axis,
                         container_writing_direction, self_writing_direction);
    regions[index + 1] =
        ToPhysicalRegion(SecondEnd(), second_axis, container_writing_direction,
                         self_writing_direction);
  }
  if (regions[0] == InsetAreaRegion::kBottom ||
      regions[1] == InsetAreaRegion::kTop) {
    std::swap(regions[0], regions[1]);
  }
  if (regions[2] == InsetAreaRegion::kRight ||
      regions[3] == InsetAreaRegion::kLeft) {
    std::swap(regions[2], regions[3]);
  }
  return InsetArea(regions[0], regions[1], regions[2], regions[3]);
}

std::optional<AnchorQuery> InsetArea::UsedTop() const {
  switch (FirstStart()) {
    case InsetAreaRegion::kTop:
      return std::nullopt;
    case InsetAreaRegion::kCenter:
      return AnchorTop();
    case InsetAreaRegion::kBottom:
      return AnchorBottom();
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> InsetArea::UsedBottom() const {
  switch (FirstEnd()) {
    case InsetAreaRegion::kTop:
      return AnchorTop();
    case InsetAreaRegion::kCenter:
      return AnchorBottom();
    case InsetAreaRegion::kBottom:
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> InsetArea::UsedLeft() const {
  switch (SecondStart()) {
    case InsetAreaRegion::kLeft:
      return std::nullopt;
    case InsetAreaRegion::kCenter:
      return AnchorLeft();
    case InsetAreaRegion::kRight:
      return AnchorRight();
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return std::nullopt;
  }
}

std::optional<AnchorQuery> InsetArea::UsedRight() const {
  switch (SecondEnd()) {
    case InsetAreaRegion::kLeft:
      return AnchorLeft();
    case InsetAreaRegion::kCenter:
      return AnchorRight();
    case InsetAreaRegion::kRight:
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return std::nullopt;
  }
}

std::pair<ItemPosition, ItemPosition> InsetArea::AlignJustifySelfFromPhysical(
    WritingDirectionMode container_writing_direction) const {
  ItemPosition align = ItemPosition::kStart;
  ItemPosition align_reverse = ItemPosition::kEnd;
  ItemPosition justify = ItemPosition::kStart;
  ItemPosition justify_reverse = ItemPosition::kEnd;

  if ((FirstStart() == InsetAreaRegion::kTop &&
       FirstEnd() == InsetAreaRegion::kBottom) ||
      (FirstStart() == InsetAreaRegion::kCenter &&
       FirstEnd() == InsetAreaRegion::kCenter)) {
    // 'center' or 'all' should align with anchor center.
    align = align_reverse = ItemPosition::kAnchorCenter;
  } else {
    // 'top' and 'top center' aligns with end, 'bottom' and 'center bottom' with
    // start.
    if (FirstStart() == InsetAreaRegion::kTop) {
      std::swap(align, align_reverse);
    }
  }
  if ((SecondStart() == InsetAreaRegion::kLeft &&
       SecondEnd() == InsetAreaRegion::kRight) ||
      (SecondStart() == InsetAreaRegion::kCenter &&
       SecondEnd() == InsetAreaRegion::kCenter)) {
    // 'center' or 'all' should align with anchor center.
    justify = justify_reverse = ItemPosition::kAnchorCenter;
  } else {
    // 'left' and 'left center' aligns with end, 'right' and 'center right' with
    // start.
    if (SecondStart() == InsetAreaRegion::kLeft) {
      std::swap(justify, justify_reverse);
    }
  }

  PhysicalToLogical converter(container_writing_direction, align,
                              justify_reverse, align_reverse, justify);
  return std::make_pair<ItemPosition, ItemPosition>(converter.BlockStart(),
                                                    converter.InlineStart());
}

AnchorQuery InsetArea::AnchorTop() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kTop);
}

AnchorQuery InsetArea::AnchorBottom() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kBottom);
}

AnchorQuery InsetArea::AnchorLeft() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kLeft);
}

AnchorQuery InsetArea::AnchorRight() {
  return AnchorQuery(CSSAnchorQueryType::kAnchor,
                     AnchorSpecifierValue::Default(), /* percentage */ 0,
                     CSSAnchorValue::kRight);
}

}  // namespace blink
