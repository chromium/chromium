// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class LayoutPoint;
class LayoutSize;
struct LogicalOffset;
struct PhysicalSize;

// PhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT PhysicalOffset {
  constexpr PhysicalOffset() = default;
  constexpr PhysicalOffset(LayoutUnit left, LayoutUnit top)
      : left(left), top(top) {}

  // For testing only. It's defined in core/testing/core_unit_test_helpers.h.
  inline PhysicalOffset(int left, int top);

  LayoutUnit left;
  LayoutUnit top;

  // Converts a physical offset to a logical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  LogicalOffset ConvertToLogical(WritingMode,
                                 TextDirection,
                                 PhysicalSize outer_size,
                                 PhysicalSize inner_size) const;

  constexpr bool IsZero() const { return !left && !top; }
  constexpr bool HasFraction() const {
    return left.HasFraction() || top.HasFraction();
  }

  void ClampNegativeToZero() {
    left = std::max(left, LayoutUnit());
    top = std::max(top, LayoutUnit());
  }

  PhysicalOffset operator+(const PhysicalOffset& other) const {
    return PhysicalOffset{this->left + other.left, this->top + other.top};
  }
  PhysicalOffset& operator+=(const PhysicalOffset& other) {
    *this = *this + other;
    return *this;
  }

  PhysicalOffset operator-() const {
    return PhysicalOffset{-this->left, -this->top};
  }
  PhysicalOffset operator-(const PhysicalOffset& other) const {
    return PhysicalOffset{this->left - other.left, this->top - other.top};
  }
  PhysicalOffset& operator-=(const PhysicalOffset& other) {
    *this = *this - other;
    return *this;
  }

  constexpr bool operator==(const PhysicalOffset& other) const {
    return other.left == left && other.top == top;
  }

  constexpr bool operator!=(const PhysicalOffset& other) const {
    return !(*this == other);
  }

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  constexpr explicit PhysicalOffset(const LayoutPoint& point)
      : left(point.X()), top(point.Y()) {}
  constexpr explicit PhysicalOffset(const LayoutSize& size)
      : left(size.Width()), top(size.Height()) {}

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  constexpr LayoutPoint ToLayoutPoint() const { return {left, top}; }
  constexpr LayoutSize ToLayoutSize() const { return {left, top}; }

  explicit PhysicalOffset(const IntPoint& point)
      : left(point.X()), top(point.Y()) {}
  explicit PhysicalOffset(const IntSize& size)
      : left(size.Width()), top(size.Height()) {}

  static PhysicalOffset FromFloatPointFloor(const FloatPoint& point) {
    return {LayoutUnit::FromFloatFloor(point.X()),
            LayoutUnit::FromFloatFloor(point.Y())};
  }
  static PhysicalOffset FromFloatPointRound(const FloatPoint& point) {
    return {LayoutUnit::FromFloatRound(point.X()),
            LayoutUnit::FromFloatRound(point.Y())};
  }
  static PhysicalOffset FromFloatSizeFloor(const FloatSize& size) {
    return {LayoutUnit::FromFloatFloor(size.Width()),
            LayoutUnit::FromFloatFloor(size.Height())};
  }
  static PhysicalOffset FromFloatSizeRound(const FloatSize& size) {
    return {LayoutUnit::FromFloatRound(size.Width()),
            LayoutUnit::FromFloatRound(size.Height())};
  }

  constexpr explicit operator FloatPoint() const { return {left, top}; }
  constexpr explicit operator FloatSize() const { return {left, top}; }

  String ToString() const;
};

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline IntPoint RoundedIntPoint(const PhysicalOffset& o) {
  return {o.left.Round(), o.top.Round()};
}
inline IntPoint FlooredIntPoint(const PhysicalOffset& o) {
  return {o.left.Floor(), o.top.Floor()};
}
inline IntPoint CeiledIntPoint(const PhysicalOffset& o) {
  return {o.left.Ceil(), o.top.Ceil()};
}

// TODO(wangxianzhu): For temporary conversion from LayoutPoint/LayoutSize to
// PhysicalOffset, where the input will be changed to PhysicalOffset soon, to
// avoid redundant PhysicalOffset() which can't be discovered by the compiler.
inline PhysicalOffset PhysicalOffsetToBeNoop(const LayoutPoint& p) {
  return PhysicalOffset(p);
}
inline PhysicalOffset PhysicalOffsetToBeNoop(const LayoutSize& s) {
  return PhysicalOffset(s);
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
