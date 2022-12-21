// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_direction_aware_resolver.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace {

template <size_t size>
using LogicalMapping = CSSDirectionAwareResolver::LogicalMapping<size>;
template <size_t size>
using PhysicalMapping = CSSDirectionAwareResolver::PhysicalMapping<size>;

enum PhysicalAxis { kPhysicalAxisX, kPhysicalAxisY };
enum PhysicalBoxSide { kTopSide, kRightSide, kBottomSide, kLeftSide };
enum PhysicalBoxCorner {
  kTopLeftCorner,
  kTopRightCorner,
  kBottomRightCorner,
  kBottomLeftCorner
};

}  // namespace

template <size_t size>
CSSDirectionAwareResolver::Group<size>::Group(
    const StylePropertyShorthand& shorthand)
    : properties_(shorthand.properties()) {
  DCHECK_EQ(size, shorthand.length());
}

template <size_t size>
CSSDirectionAwareResolver::Group<size>::Group(
    const CSSProperty* (&properties)[size])
    : properties_(properties) {}

template <size_t size>
const CSSProperty& CSSDirectionAwareResolver::Group<size>::GetProperty(
    size_t index) const {
  DCHECK_LT(index, size);
  return *properties_[index];
}

template <size_t size>
bool CSSDirectionAwareResolver::Group<size>::Contains(CSSPropertyID id) const {
  for (size_t i = 0; i < size; ++i) {
    if (properties_[i]->IDEquals(id)) {
      return true;
    }
  }
  return false;
}

template class CSSDirectionAwareResolver::Group<2ul>;
template class CSSDirectionAwareResolver::Group<4ul>;

LogicalMapping<4> CSSDirectionAwareResolver::LogicalBorderMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderBlockStart(), &GetCSSPropertyBorderBlockEnd(),
      &GetCSSPropertyBorderInlineStart(), &GetCSSPropertyBorderInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderTop(), &GetCSSPropertyBorderRight(),
      &GetCSSPropertyBorderBottom(), &GetCSSPropertyBorderLeft()};
  return PhysicalMapping<4>(kProperties);
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalBorderColorMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderBlockStartColor(),
      &GetCSSPropertyBorderBlockEndColor(),
      &GetCSSPropertyBorderInlineStartColor(),
      &GetCSSPropertyBorderInlineEndColor()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderColorMapping() {
  return PhysicalMapping<4>(borderColorShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalBorderStyleMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderBlockStartStyle(),
      &GetCSSPropertyBorderBlockEndStyle(),
      &GetCSSPropertyBorderInlineStartStyle(),
      &GetCSSPropertyBorderInlineEndStyle()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderStyleMapping() {
  return PhysicalMapping<4>(borderStyleShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalBorderWidthMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderBlockStartWidth(),
      &GetCSSPropertyBorderBlockEndWidth(),
      &GetCSSPropertyBorderInlineStartWidth(),
      &GetCSSPropertyBorderInlineEndWidth()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<2>
CSSDirectionAwareResolver::PhysicalContainIntrinsicSizeMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyContainIntrinsicWidth(),
      &GetCSSPropertyContainIntrinsicHeight()};
  return PhysicalMapping<2>(kProperties);
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalBorderRadiusMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderStartStartRadius(),
      &GetCSSPropertyBorderStartEndRadius(),
      &GetCSSPropertyBorderEndStartRadius(),
      &GetCSSPropertyBorderEndEndRadius()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderRadiusMapping() {
  return PhysicalMapping<4>(borderRadiusShorthand());
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderWidthMapping() {
  return PhysicalMapping<4>(borderWidthShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalInsetMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyInsetBlockStart(), &GetCSSPropertyInsetBlockEnd(),
      &GetCSSPropertyInsetInlineStart(), &GetCSSPropertyInsetInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalInsetMapping() {
  return PhysicalMapping<4>(insetShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalMarginMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyMarginBlockStart(), &GetCSSPropertyMarginBlockEnd(),
      &GetCSSPropertyMarginInlineStart(), &GetCSSPropertyMarginInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalMarginMapping() {
  return PhysicalMapping<4>(marginShorthand());
}

LogicalMapping<2> CSSDirectionAwareResolver::LogicalMaxSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMaxBlockSize(),
                                             &GetCSSPropertyMaxInlineSize()};
  return LogicalMapping<2>(kProperties);
}

PhysicalMapping<2> CSSDirectionAwareResolver::PhysicalMaxSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMaxWidth(),
                                             &GetCSSPropertyMaxHeight()};
  return PhysicalMapping<2>(kProperties);
}

LogicalMapping<2> CSSDirectionAwareResolver::LogicalMinSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMinBlockSize(),
                                             &GetCSSPropertyMinInlineSize()};
  return LogicalMapping<2>(kProperties);
}

PhysicalMapping<2> CSSDirectionAwareResolver::PhysicalMinSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMinWidth(),
                                             &GetCSSPropertyMinHeight()};
  return PhysicalMapping<2>(kProperties);
}

LogicalMapping<2> CSSDirectionAwareResolver::LogicalOverflowMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyOverflowBlock(),
                                             &GetCSSPropertyOverflowInline()};
  return LogicalMapping<2>(kProperties);
}

PhysicalMapping<2> CSSDirectionAwareResolver::PhysicalOverflowMapping() {
  return PhysicalMapping<2>(overflowShorthand());
}

LogicalMapping<2>
CSSDirectionAwareResolver::LogicalOverscrollBehaviorMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyOverscrollBehaviorBlock(),
      &GetCSSPropertyOverscrollBehaviorInline()};
  return LogicalMapping<2>(kProperties);
}

PhysicalMapping<2>
CSSDirectionAwareResolver::PhysicalOverscrollBehaviorMapping() {
  return PhysicalMapping<2>(overscrollBehaviorShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalPaddingMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyPaddingBlockStart(), &GetCSSPropertyPaddingBlockEnd(),
      &GetCSSPropertyPaddingInlineStart(), &GetCSSPropertyPaddingInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalPaddingMapping() {
  return PhysicalMapping<4>(paddingShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalScrollMarginMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyScrollMarginBlockStart(),
      &GetCSSPropertyScrollMarginBlockEnd(),
      &GetCSSPropertyScrollMarginInlineStart(),
      &GetCSSPropertyScrollMarginInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalScrollMarginMapping() {
  return PhysicalMapping<4>(scrollMarginShorthand());
}

LogicalMapping<4> CSSDirectionAwareResolver::LogicalScrollPaddingMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyScrollPaddingBlockStart(),
      &GetCSSPropertyScrollPaddingBlockEnd(),
      &GetCSSPropertyScrollPaddingInlineStart(),
      &GetCSSPropertyScrollPaddingInlineEnd()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalScrollPaddingMapping() {
  return PhysicalMapping<4>(scrollPaddingShorthand());
}

LogicalMapping<2> CSSDirectionAwareResolver::LogicalSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyBlockSize(),
                                             &GetCSSPropertyInlineSize()};
  return LogicalMapping<2>(kProperties);
}

PhysicalMapping<2> CSSDirectionAwareResolver::PhysicalSizeMapping() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyWidth(),
                                             &GetCSSPropertyHeight()};
  return PhysicalMapping<2>(kProperties);
}

LogicalMapping<4>
CSSDirectionAwareResolver::LogicalVisitedBorderColorMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyInternalVisitedBorderBlockStartColor(),
      &GetCSSPropertyInternalVisitedBorderBlockEndColor(),
      &GetCSSPropertyInternalVisitedBorderInlineStartColor(),
      &GetCSSPropertyInternalVisitedBorderInlineEndColor()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4>
CSSDirectionAwareResolver::PhysicalVisitedBorderColorMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyInternalVisitedBorderTopColor(),
      &GetCSSPropertyInternalVisitedBorderRightColor(),
      &GetCSSPropertyInternalVisitedBorderBottomColor(),
      &GetCSSPropertyInternalVisitedBorderLeftColor()};
  return PhysicalMapping<4>(kProperties);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInlineStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode)) {
      return group.GetProperty(kLeftSide);
    }
    return group.GetProperty(kTopSide);
  }
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kRightSide);
  }
  return group.GetProperty(kBottomSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInlineEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode)) {
      return group.GetProperty(kRightSide);
    }
    return group.GetProperty(kBottomSide);
  }
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kLeftSide);
  }
  return group.GetProperty(kTopSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kTopSide);
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kLeftSide);
  }
  return group.GetProperty(kRightSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kBottomSide);
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kRightSide);
  }
  return group.GetProperty(kLeftSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInline(
    TextDirection,
    WritingMode writing_mode,
    const PhysicalMapping<2>& group) {
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kPhysicalAxisX);
  }
  return group.GetProperty(kPhysicalAxisY);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlock(
    TextDirection,
    WritingMode writing_mode,
    const PhysicalMapping<2>& group) {
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kPhysicalAxisY);
  }
  return group.GetProperty(kPhysicalAxisX);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveStartStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode) ||
        IsFlippedLinesWritingMode(writing_mode)) {
      return group.GetProperty(kTopLeftCorner);
    }
    return group.GetProperty(kTopRightCorner);
  }
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kTopRightCorner);
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kBottomLeftCorner);
  }
  return group.GetProperty(kBottomRightCorner);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveStartEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode)) {
      return group.GetProperty(kTopRightCorner);
    }
    if (IsFlippedLinesWritingMode(writing_mode)) {
      return group.GetProperty(kBottomLeftCorner);
    }
    return group.GetProperty(kBottomRightCorner);
  }
  if (IsHorizontalWritingMode(writing_mode) ||
      IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kTopLeftCorner);
  }
  return group.GetProperty(kTopRightCorner);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveEndStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode)) {
      return group.GetProperty(kBottomLeftCorner);
    }
    if (IsFlippedLinesWritingMode(writing_mode)) {
      return group.GetProperty(kTopRightCorner);
    }
    return group.GetProperty(kTopLeftCorner);
  }
  if (IsHorizontalWritingMode(writing_mode) ||
      IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kBottomRightCorner);
  }
  return group.GetProperty(kBottomLeftCorner);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveEndEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalMapping<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode) ||
        IsFlippedLinesWritingMode(writing_mode)) {
      return group.GetProperty(kBottomRightCorner);
    }
    return group.GetProperty(kBottomLeftCorner);
  }
  if (IsHorizontalWritingMode(writing_mode)) {
    return group.GetProperty(kBottomLeftCorner);
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return group.GetProperty(kTopRightCorner);
  }
  return group.GetProperty(kTopLeftCorner);
}

}  // namespace blink
