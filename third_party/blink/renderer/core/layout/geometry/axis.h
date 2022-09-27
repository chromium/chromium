// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_

#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

using PhysicalAxes = base::StrongAlias<class PhysicalAxesTag, uint8_t>;
using LogicalAxes = base::StrongAlias<class LogicalAxesTag, uint8_t>;

inline constexpr LogicalAxes operator|(LogicalAxes a, LogicalAxes b) {
  return LogicalAxes(a.value() | b.value());
}

inline constexpr LogicalAxes& operator|=(LogicalAxes& a, LogicalAxes b) {
  a.value() |= b.value();
  return a;
}

inline constexpr LogicalAxes operator&(LogicalAxes a, LogicalAxes b) {
  return LogicalAxes(a.value() & b.value());
}

inline constexpr LogicalAxes operator&=(LogicalAxes& a, LogicalAxes b) {
  a.value() &= b.value();
  return a;
}

inline constexpr PhysicalAxes operator|(PhysicalAxes a, PhysicalAxes b) {
  return PhysicalAxes(a.value() | b.value());
}

inline constexpr PhysicalAxes& operator|=(PhysicalAxes& a, PhysicalAxes b) {
  a.value() |= b.value();
  return a;
}

inline constexpr PhysicalAxes operator&(PhysicalAxes a, PhysicalAxes b) {
  return PhysicalAxes(a.value() & b.value());
}

inline constexpr PhysicalAxes operator&=(PhysicalAxes& a, PhysicalAxes b) {
  a.value() &= b.value();
  return a;
}

inline constexpr LogicalAxes kLogicalAxisNone = LogicalAxes(0);
inline constexpr LogicalAxes kLogicalAxisInline = LogicalAxes(1 << 0);
inline constexpr LogicalAxes kLogicalAxisBlock = LogicalAxes(1 << 1);
inline constexpr LogicalAxes kLogicalAxisBoth =
    kLogicalAxisInline | kLogicalAxisBlock;

inline constexpr PhysicalAxes kPhysicalAxisNone = PhysicalAxes(0);
inline constexpr PhysicalAxes kPhysicalAxisHorizontal = PhysicalAxes(1 << 0);
inline constexpr PhysicalAxes kPhysicalAxisVertical = PhysicalAxes(1 << 1);
inline constexpr PhysicalAxes kPhysicalAxisBoth =
    kPhysicalAxisHorizontal | kPhysicalAxisVertical;

// ConvertAxes relies on the fact that the underlying values for
// for Inline/Horizontal are the same, and that the underlying values for
// Block/Vertical are the same.
static_assert(kLogicalAxisNone.value() == kPhysicalAxisNone.value());
static_assert(kLogicalAxisInline.value() == kPhysicalAxisHorizontal.value());
static_assert(kLogicalAxisBlock.value() == kPhysicalAxisVertical.value());
static_assert(kLogicalAxisBoth.value() == kPhysicalAxisBoth.value());

template <typename FromType, typename ToType>
inline ToType ConvertAxes(FromType from, WritingMode mode) {
  // Reverse the bits if |mode| is a vertical writing mode.
  int shift = !IsHorizontalWritingMode(mode);
  return ToType(((from.value() >> shift) & 1) | ((from.value() << shift) & 2));
}

inline PhysicalAxes ToPhysicalAxes(LogicalAxes logical, WritingMode mode) {
  return ConvertAxes<LogicalAxes, PhysicalAxes>(logical, mode);
}

inline LogicalAxes ToLogicalAxes(PhysicalAxes physical, WritingMode mode) {
  return ConvertAxes<PhysicalAxes, LogicalAxes>(physical, mode);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_
