// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_direction_aware_resolver.h"

#include <array>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {
namespace {

template <size_t size>
using LogicalMapping = CSSDirectionAwareResolver::LogicalMapping<size>;
template <size_t size>
using PhysicalMapping = CSSDirectionAwareResolver::PhysicalMapping<size>;

enum PhysicalAxis { kPhysicalAxisX, kPhysicalAxisY };
enum PhysicalBoxCorner {
  kTopLeftCorner,
  kTopRightCorner,
  kBottomRightCorner,
  kBottomLeftCorner
};

enum LogicalAxis { kBlock, kInline };
enum LogicalBoxCorner { kStartStart, kStartEnd, kEndStart, kEndEnd };

constexpr size_t kWritingModeSize =
    static_cast<size_t>(WritingMode::kMaxWritingMode) + 1;
// Following eight arrays contain values for horizontal-tb, vertical-rl,
// vertical-lr, sideways-rl, and sideways-lr in this order.
constexpr std::array<uint8_t, kWritingModeSize> kStartStartMap = {
    kTopLeftCorner,  kTopRightCorner,   kTopLeftCorner,
    kTopRightCorner, kBottomLeftCorner,
};
constexpr std::array<uint8_t, kWritingModeSize> kStartEndMap = {
    kTopRightCorner,    kBottomRightCorner, kBottomLeftCorner,
    kBottomRightCorner, kTopLeftCorner,
};
constexpr std::array<uint8_t, kWritingModeSize> kEndStartMap = {
    kBottomLeftCorner, kTopLeftCorner,     kTopRightCorner,
    kTopLeftCorner,    kBottomRightCorner,
};
constexpr std::array<uint8_t, kWritingModeSize> kEndEndMap = {
    kBottomRightCorner, kBottomLeftCorner, kBottomRightCorner,
    kBottomLeftCorner,  kTopRightCorner,
};
constexpr std::array<uint8_t, kWritingModeSize> kTopLeftMap = {
    kStartStart, kEndStart, kStartStart, kEndStart, kStartEnd,
};
constexpr std::array<uint8_t, kWritingModeSize> kTopRightMap = {
    kStartEnd, kStartStart, kEndStart, kStartStart, kEndEnd,
};
constexpr std::array<uint8_t, kWritingModeSize> kBottomRightMap = {
    kEndEnd, kStartEnd, kEndEnd, kStartEnd, kEndStart,
};
constexpr std::array<uint8_t, kWritingModeSize> kBottomLeftMap = {
    kEndStart, kEndEnd, kStartEnd, kEndEnd, kStartStart,
};

// Prerequisites for Physical*Mapping().
STATIC_ASSERT_ENUM(PhysicalDirection::kUp, 0);
STATIC_ASSERT_ENUM(PhysicalDirection::kRight, 1);
STATIC_ASSERT_ENUM(PhysicalDirection::kDown, 2);
STATIC_ASSERT_ENUM(PhysicalDirection::kLeft, 3);

}  // namespace

template <size_t size>
CSSDirectionAwareResolver::Group<size>::Group(
    const StylePropertyShorthand& shorthand)
    : properties_(shorthand.properties()) {
  DCHECK_EQ(size, shorthand.length());
}

template <size_t size>
CSSDirectionAwareResolver::Group<size>::Group(
    const CSSProperty* const (&properties)[size])
    : properties_(properties) {}

template <size_t size>
const CSSProperty& CSSDirectionAwareResolver::Group<size>::GetProperty(
    size_t index) const {
  DCHECK_LT(index, size);
  return *properties_[index];
}

template <size_t size>
bool CSSDirectionAwareResolver::Group<size>::Contains(CSSPropertyID id) const {
  for (const CSSProperty* property : properties_) {
    if (property->IDEquals(id)) {
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

LogicalMapping<4> CSSDirectionAwareResolver::LogicalCornerShapeMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyCornerStartStartShape(),
      &GetCSSPropertyCornerStartEndShape(),
      &GetCSSPropertyCornerEndStartShape(), &GetCSSPropertyCornerEndEndShape()};
  return LogicalMapping<4>(kProperties);
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalBorderRadiusMapping() {
  return PhysicalMapping<4>(borderRadiusShorthand());
}

PhysicalMapping<4> CSSDirectionAwareResolver::PhysicalCornerShapeMapping() {
  static const CSSProperty* kProperties[] = {
      &GetCSSPropertyCornerTopLeftShape(), &GetCSSPropertyCornerTopRightShape(),
      &GetCSSPropertyCornerBottomRightShape(),
      &GetCSSPropertyCornerBottomLeftShape()};
  return PhysicalMapping<4>(kProperties);
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
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  return group.GetProperty(
      static_cast<size_t>(writing_direction.InlineStart()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInlineEnd(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.InlineEnd()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockStart(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.BlockStart()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlockEnd(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.BlockEnd()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveTop(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.Top()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveRight(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.Right()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBottom(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.Bottom()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveLeft(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  return group.GetProperty(static_cast<size_t>(writing_direction.Left()));
}

const CSSProperty& CSSDirectionAwareResolver::ResolveInline(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<2>& group) {
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kPhysicalAxisX);
  }
  return group.GetProperty(kPhysicalAxisY);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBlock(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<2>& group) {
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kPhysicalAxisY);
  }
  return group.GetProperty(kPhysicalAxisX);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveHorizontal(
    WritingDirectionMode writing_direction,
    const LogicalMapping<2>& group) {
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kInline);
  }
  return group.GetProperty(kBlock);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveVertical(
    WritingDirectionMode writing_direction,
    const LogicalMapping<2>& group) {
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kBlock);
  }
  return group.GetProperty(kInline);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveStartStart(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kStartStartMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kStartEndMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveStartEnd(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kStartEndMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kStartStartMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveEndStart(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kEndStartMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kEndEndMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveEndEnd(
    WritingDirectionMode writing_direction,
    const PhysicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kEndEndMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kEndStartMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveTopLeft(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kTopLeftMap[static_cast<int>(writing_mode)]);
  }
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kTopRightMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kBottomLeftMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveTopRight(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kTopRightMap[static_cast<int>(writing_mode)]);
  }
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kTopLeftMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kBottomRightMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBottomRight(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kBottomRightMap[static_cast<int>(writing_mode)]);
  }
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kBottomLeftMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kTopRightMap[static_cast<int>(writing_mode)]);
}

const CSSProperty& CSSDirectionAwareResolver::ResolveBottomLeft(
    WritingDirectionMode writing_direction,
    const LogicalMapping<4>& group) {
  WritingMode writing_mode = writing_direction.GetWritingMode();
  if (writing_direction.IsLtr()) {
    return group.GetProperty(kBottomLeftMap[static_cast<int>(writing_mode)]);
  }
  if (writing_direction.IsHorizontal()) {
    return group.GetProperty(kBottomRightMap[static_cast<int>(writing_mode)]);
  }
  return group.GetProperty(kTopLeftMap[static_cast<int>(writing_mode)]);
}

}  // namespace blink
