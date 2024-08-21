// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

class LayoutPoint;
struct LogicalOffset;
struct PhysicalSize;

// PhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT PhysicalOffset {
  constexpr PhysicalOffset() = default;
  constexpr PhysicalOffset(LayoutUnit left, LayoutUnit top)
      : left(left), top(top) {}

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter,
  // or use FromPointF*() instead.
  PhysicalOffset(double, double) = delete;

  // For testing only. It's defined in core/testing/core_unit_test_helper.h.
  // 'constexpr' is to let compiler detect usage from production code.
  constexpr PhysicalOffset(int left, int top);

  LayoutUnit left;
  LayoutUnit top;

  // Converts a physical offset to a logical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  LogicalOffset ConvertToLogical(WritingDirectionMode writing_direction,
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

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  constexpr LayoutPoint ToLayoutPoint() const { return {left, top}; }

  explicit PhysicalOffset(const gfx::Point& point)
      : left(point.x()), top(point.y()) {}
  explicit PhysicalOffset(const gfx::Vector2d& vector)
      : left(vector.x()), top(vector.y()) {}

  static PhysicalOffset FromPointFFloor(const gfx::PointF& point) {
    return {LayoutUnit::FromFloatFloor(point.x()),
            LayoutUnit::FromFloatFloor(point.y())};
  }
  static PhysicalOffset FromPointFRound(const gfx::PointF& point) {
    return {LayoutUnit::FromFloatRound(point.x()),
            LayoutUnit::FromFloatRound(point.y())};
  }
  static PhysicalOffset FromVector2dFFloor(const gfx::Vector2dF& vector) {
    return {LayoutUnit::FromFloatFloor(vector.x()),
            LayoutUnit::FromFloatFloor(vector.y())};
  }
  static PhysicalOffset FromVector2dFRound(const gfx::Vector2dF& vector) {
    return {LayoutUnit::FromFloatRound(vector.x()),
            LayoutUnit::FromFloatRound(vector.y())};
  }

  void Scale(float s) {
    left *= s;
    top *= s;
  }

  constexpr explicit operator gfx::PointF() const { return {left, top}; }
  constexpr explicit operator gfx::Vector2dF() const { return {left, top}; }

  WTF::String ToString() const;
};

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline gfx::Point ToRoundedPoint(const PhysicalOffset& o) {
  return {o.left.Round(), o.top.Round()};
}
inline gfx::Point ToFlooredPoint(const PhysicalOffset& o) {
  return {o.left.Floor(), o.top.Floor()};
}
inline gfx::Point ToCeiledPoint(const PhysicalOffset& o) {
  return {o.left.Ceil(), o.top.Ceil()};
}

inline gfx::Vector2d ToRoundedVector2d(const PhysicalOffset& o) {
  return {o.left.Round(), o.top.Round()};
}
inline gfx::Vector2d ToFlooredVector2d(const PhysicalOffset& o) {
  return {o.left.Floor(), o.top.Floor()};
}
inline gfx::Vector2d ToCeiledVector2d(const PhysicalOffset& o) {
  return {o.left.Ceil(), o.top.Ceil()};
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
