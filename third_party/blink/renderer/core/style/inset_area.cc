// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/inset_area.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"

namespace blink {

CORE_EXPORT DEFINE_GLOBAL(Length, g_anchor_top_length);
CORE_EXPORT DEFINE_GLOBAL(Length, g_anchor_bottom_length);
CORE_EXPORT DEFINE_GLOBAL(Length, g_anchor_left_length);
CORE_EXPORT DEFINE_GLOBAL(Length, g_anchor_right_length);

namespace {

inline PhysicalAxes PhysicalAxisFromRegion(InsetAreaRegion region) {
  switch (region) {
    case InsetAreaRegion::kTop:
    case InsetAreaRegion::kBottom:
    case InsetAreaRegion::kYStart:
    case InsetAreaRegion::kYEnd:
    case InsetAreaRegion::kYSelfStart:
    case InsetAreaRegion::kYSelfEnd:
      return kPhysicalAxisVertical;
    case InsetAreaRegion::kLeft:
    case InsetAreaRegion::kRight:
    case InsetAreaRegion::kXStart:
    case InsetAreaRegion::kXEnd:
    case InsetAreaRegion::kXSelfStart:
    case InsetAreaRegion::kXSelfEnd:
      return kPhysicalAxisHorizontal;
    default:
      // Neutral region. Axis depends on the other span or order of appearance
      // if both spans are neutral.
      return kPhysicalAxisNone;
  }
}

// Return the physical axis for an inset-area span if given by the regions, or
// kPhysicalAxisNone if we need the direction/writing-mode to decide.
inline PhysicalAxes PhysicalAxisFromSpan(InsetAreaRegion start,
                                         InsetAreaRegion end) {
  if (start == InsetAreaRegion::kAll) {
    return kPhysicalAxisNone;
  }
  InsetAreaRegion indicator = start == InsetAreaRegion::kCenter ? end : start;
  return PhysicalAxisFromRegion(indicator);
}

// Convert a logical region to the corresponding physical region based on the
// span's axis and the direction/writing-mode of the anchored element and its
// containing block.
InsetAreaRegion ToPhysicalRegion(
    InsetAreaRegion region,
    PhysicalAxes axis,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction) {
  bool is_horizontal = axis == kPhysicalAxisHorizontal;
  InsetAreaRegion axis_region = region;
  switch (region) {
    case InsetAreaRegion::kNone:
    case InsetAreaRegion::kAll:
      NOTREACHED() << "Should be handled directly in InsetArea::ToPhysical";
      [[fallthrough]];
    case InsetAreaRegion::kCenter:
    case InsetAreaRegion::kTop:
    case InsetAreaRegion::kBottom:
    case InsetAreaRegion::kLeft:
    case InsetAreaRegion::kRight:
      return region;
    case InsetAreaRegion::kStart:
      axis_region =
          is_horizontal ? InsetAreaRegion::kXStart : InsetAreaRegion::kYStart;
      break;
    case InsetAreaRegion::kEnd:
      axis_region =
          is_horizontal ? InsetAreaRegion::kXEnd : InsetAreaRegion::kYEnd;
      break;
    case InsetAreaRegion::kSelfStart:
      axis_region = is_horizontal ? InsetAreaRegion::kXSelfStart
                                  : InsetAreaRegion::kYSelfStart;
      break;
    case InsetAreaRegion::kSelfEnd:
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
  PhysicalAxes first_axis = PhysicalAxisFromSpan(FirstStart(), FirstEnd());
  PhysicalAxes second_axis = PhysicalAxisFromSpan(SecondStart(), SecondEnd());

  if (first_axis == second_axis) {
    if (first_axis != kPhysicalAxisNone) {
      // Both regions representing the same axis is invalid
      return InsetArea();
    }
    // If neither span includes a physical keyword, the first refers to the
    // block axis of the containing block, and the second to the inline axis.
    first_axis = ToPhysicalAxes(kLogicalAxisBlock,
                                container_writing_direction.GetWritingMode());
    second_axis = ToPhysicalAxes(kLogicalAxisInline,
                                 container_writing_direction.GetWritingMode());
  } else {
    if (first_axis == kPhysicalAxisNone) {
      first_axis = second_axis ^ kPhysicalAxisBoth;
    } else if (second_axis == kPhysicalAxisNone) {
      second_axis = first_axis ^ kPhysicalAxisBoth;
    }
  }
  DCHECK_EQ(first_axis ^ second_axis, kPhysicalAxisBoth)
      << "Both axes should be defined and orthogonal";

  InsetAreaRegion regions[4] = {InsetAreaRegion::kTop, InsetAreaRegion::kBottom,
                                InsetAreaRegion::kLeft,
                                InsetAreaRegion::kRight};

  // Adjust the index to always make the first span the vertical one in the
  // resulting InsetArea, regardless of the original ordering.
  size_t index = first_axis == kPhysicalAxisHorizontal ? 2 : 0;
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

const Length& InsetArea::UsedTop() const {
  switch (FirstStart()) {
    case InsetAreaRegion::kTop:
      return Length::FixedZero();
    case InsetAreaRegion::kCenter:
      return g_anchor_top_length;
    case InsetAreaRegion::kBottom:
      return g_anchor_bottom_length;
    default:
      NOTREACHED();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return Length::Auto();
  }
}

const Length& InsetArea::UsedBottom() const {
  switch (FirstEnd()) {
    case InsetAreaRegion::kTop:
      return g_anchor_top_length;
    case InsetAreaRegion::kCenter:
      return g_anchor_bottom_length;
    case InsetAreaRegion::kBottom:
      return Length::FixedZero();
    default:
      NOTREACHED();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return Length::Auto();
  }
}

const Length& InsetArea::UsedLeft() const {
  switch (SecondStart()) {
    case InsetAreaRegion::kLeft:
      return Length::FixedZero();
    case InsetAreaRegion::kCenter:
      return g_anchor_left_length;
    case InsetAreaRegion::kRight:
      return g_anchor_right_length;
    default:
      NOTREACHED();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return Length::Auto();
  }
}

const Length& InsetArea::UsedRight() const {
  switch (SecondEnd()) {
    case InsetAreaRegion::kLeft:
      return g_anchor_left_length;
    case InsetAreaRegion::kCenter:
      return g_anchor_right_length;
    case InsetAreaRegion::kRight:
      return Length::FixedZero();
    default:
      NOTREACHED();
      [[fallthrough]];
    case InsetAreaRegion::kNone:
      return Length::Auto();
  }
}

void InsetArea::InitializeAnchorLengths() {
  // These globals are initialized here instead of Length::Initialize() because
  // they depend on anchor expressions defined in core/ which cannot be included
  // from platform.
  new (WTF::NotNullTag::kNotNull, (void*)&g_anchor_top_length)
      Length(CalculationValue::CreateSimplified(
          CalculationExpressionAnchorQueryNode::CreateAnchor(
              *AnchorSpecifierValue::Default(), CSSAnchorValue::kTop,
              Length::FixedZero()),
          Length::ValueRange::kAll));
  new (WTF::NotNullTag::kNotNull, (void*)&g_anchor_bottom_length)
      Length(CalculationValue::CreateSimplified(
          CalculationExpressionAnchorQueryNode::CreateAnchor(
              *AnchorSpecifierValue::Default(), CSSAnchorValue::kBottom,
              Length::FixedZero()),
          Length::ValueRange::kAll));
  new (WTF::NotNullTag::kNotNull, (void*)&g_anchor_left_length)
      Length(CalculationValue::CreateSimplified(
          CalculationExpressionAnchorQueryNode::CreateAnchor(
              *AnchorSpecifierValue::Default(), CSSAnchorValue::kLeft,
              Length::FixedZero()),
          Length::ValueRange::kAll));
  new (WTF::NotNullTag::kNotNull, (void*)&g_anchor_right_length)
      Length(CalculationValue::CreateSimplified(
          CalculationExpressionAnchorQueryNode::CreateAnchor(
              *AnchorSpecifierValue::Default(), CSSAnchorValue::kRight,
              Length::FixedZero()),
          Length::ValueRange::kAll));
}

}  // namespace blink
