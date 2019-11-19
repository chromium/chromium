// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_direction_aware_resolver.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace {

template <size_t size>
using PhysicalGroup = CSSDirectionAwareResolver::PhysicalGroup<size>;

enum PhysicalAxis { kPhysicalAxisX, kPhysicalAxisY };
enum PhysicalBoxSide { kTopSide, kRightSide, kBottomSide, kLeftSide };

}  // namespace

template <size_t size>
CSSDirectionAwareResolver::PhysicalGroup<size>::PhysicalGroup(
    const StylePropertyShorthand& shorthand)
    : properties_(shorthand.properties()) {
  DCHECK_EQ(size, shorthand.length());
}

template <size_t size>
CSSDirectionAwareResolver::PhysicalGroup<size>::PhysicalGroup(
    const CSSProperty* (&properties)[size])
    : properties_(properties) {}

template <size_t size>
const CSSProperty& CSSDirectionAwareResolver::PhysicalGroup<size>::GetProperty(
    size_t index) const {
  DCHECK_LT(index, size);
  return *properties_[index];
}

PhysicalGroup<4> CSSDirectionAwareResolver::BorderGroup() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyBorderTop(), &GetCSSPropertyBorderRight(),
      &GetCSSPropertyBorderBottom(), &GetCSSPropertyBorderLeft()};
  return PhysicalGroup<4>(kProperties);
}

PhysicalGroup<4> CSSDirectionAwareResolver::BorderColorGroup() {
  return PhysicalGroup<4>(borderColorShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::BorderStyleGroup() {
  return PhysicalGroup<4>(borderStyleShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::BorderWidthGroup() {
  return PhysicalGroup<4>(borderWidthShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::InsetGroup() {
  return PhysicalGroup<4>(insetShorthand());
}

PhysicalGroup<2> CSSDirectionAwareResolver::IntrinsicSizeGroup() {
  return PhysicalGroup<2>(intrinsicSizeShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::MarginGroup() {
  return PhysicalGroup<4>(marginShorthand());
}

PhysicalGroup<2> CSSDirectionAwareResolver::MaxSizeGroup() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMaxWidth(),
                                             &GetCSSPropertyMaxHeight()};
  return PhysicalGroup<2>(kProperties);
}

PhysicalGroup<2> CSSDirectionAwareResolver::MinSizeGroup() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyMinWidth(),
                                             &GetCSSPropertyMinHeight()};
  return PhysicalGroup<2>(kProperties);
}

PhysicalGroup<2> CSSDirectionAwareResolver::OverflowGroup() {
  return PhysicalGroup<2>(overflowShorthand());
}

PhysicalGroup<2> CSSDirectionAwareResolver::OverscrollBehaviorGroup() {
  return PhysicalGroup<2>(overscrollBehaviorShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::PaddingGroup() {
  return PhysicalGroup<4>(paddingShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::ScrollMarginGroup() {
  return PhysicalGroup<4>(scrollMarginShorthand());
}

PhysicalGroup<4> CSSDirectionAwareResolver::ScrollPaddingGroup() {
  return PhysicalGroup<4>(scrollPaddingShorthand());
}

PhysicalGroup<2> CSSDirectionAwareResolver::SizeGroup() {
  static const CSSProperty* kProperties[] = {&GetCSSPropertyWidth(),
                                             &GetCSSPropertyHeight()};
  return PhysicalGroup<2>(kProperties);
}

PhysicalGroup<4> CSSDirectionAwareResolver::VisitedBorderColorGroup() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyInternalVisitedBorderTopColor(),
      &GetCSSPropertyInternalVisitedBorderRightColor(),
      &GetCSSPropertyInternalVisitedBorderBottomColor(),
      &GetCSSPropertyInternalVisitedBorderLeftColor()};
  return PhysicalGroup<4>(kProperties);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInlineStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalGroup<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return group.GetProperty(kLeftSide);
    return group.GetProperty(kTopSide);
  }
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kRightSide);
  return group.GetProperty(kBottomSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInlineEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalGroup<4>& group) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return group.GetProperty(kRightSide);
    return group.GetProperty(kBottomSide);
  }
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kLeftSide);
  return group.GetProperty(kTopSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockStart(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalGroup<4>& group) {
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kTopSide);
  if (IsFlippedLinesWritingMode(writing_mode))
    return group.GetProperty(kLeftSide);
  return group.GetProperty(kRightSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockEnd(
    TextDirection direction,
    WritingMode writing_mode,
    const PhysicalGroup<4>& group) {
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kBottomSide);
  if (IsFlippedLinesWritingMode(writing_mode))
    return group.GetProperty(kRightSide);
  return group.GetProperty(kLeftSide);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInline(
    TextDirection,
    WritingMode writing_mode,
    const PhysicalGroup<2>& group) {
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kPhysicalAxisX);
  return group.GetProperty(kPhysicalAxisY);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlock(
    TextDirection,
    WritingMode writing_mode,
    const PhysicalGroup<2>& group) {
  if (IsHorizontalWritingMode(writing_mode))
    return group.GetProperty(kPhysicalAxisY);
  return group.GetProperty(kPhysicalAxisX);
}

}  // namespace blink
