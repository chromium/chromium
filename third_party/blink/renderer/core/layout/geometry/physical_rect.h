// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {
class TextStream;
}

namespace blink {

class ComputedStyle;
struct LogicalRect;
struct NGPhysicalBoxStrut;

// PhysicalRect is the position and size of a rect (typically a fragment)
// relative to its parent rect in the physical coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT PhysicalRect {
  constexpr PhysicalRect() = default;
  constexpr PhysicalRect(const PhysicalOffset& offset, const PhysicalSize& size)
      : offset(offset), size(size) {}
  // TODO(wangxianzhu): This is temporary for convenience of constructing
  // PhysicalRect with LayoutBox::Size(), before we convert LayoutBox::Size() to
  // PhysicalSize.
  constexpr PhysicalRect(const PhysicalOffset& offset, const LayoutSize& size)
      : offset(offset), size(size) {}
  constexpr PhysicalRect(LayoutUnit left,
                         LayoutUnit top,
                         LayoutUnit width,
                         LayoutUnit height)
      : offset(left, top), size(width, height) {}

  // For testing only. It's defined in core/testing/core_unit_test_helpers.h.
  inline PhysicalRect(int left, int top, int width, int height);

  PhysicalOffset offset;
  PhysicalSize size;

  // Converts a physical offset to a logical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  LogicalRect ConvertToLogical(WritingMode,
                               TextDirection,
                               PhysicalSize outer_size,
                               PhysicalSize inner_size) const;

  constexpr bool IsEmpty() const { return size.IsEmpty(); }

  constexpr LayoutUnit X() const { return offset.left; }
  constexpr LayoutUnit Y() const { return offset.top; }
  constexpr LayoutUnit Width() const { return size.width; }
  constexpr LayoutUnit Height() const { return size.height; }
  LayoutUnit Right() const { return offset.left + size.width; }
  LayoutUnit Bottom() const { return offset.top + size.height; }

  void SetX(LayoutUnit x) { offset.left = x; }
  void SetY(LayoutUnit y) { offset.top = y; }
  void SetWidth(LayoutUnit w) { size.width = w; }
  void SetHeight(LayoutUnit h) { size.height = h; }

  PhysicalOffset MinXMinYCorner() const { return offset; }
  PhysicalOffset MaxXMinYCorner() const {
    return {offset.left + size.width, offset.top};
  }
  PhysicalOffset MinXMaxYCorner() const {
    return {offset.left, offset.top + size.height};
  }
  PhysicalOffset MaxXMaxYCorner() const {
    return {offset.left + size.width, offset.top + size.height};
  }

  constexpr bool operator==(const PhysicalRect& other) const {
    return offset == other.offset && size == other.size;
  }
  bool operator!=(const PhysicalRect& other) const { return !(*this == other); }

  bool Contains(const PhysicalRect&) const;
  bool Contains(LayoutUnit px, LayoutUnit py) const {
    return px >= offset.left && px < Right() && py >= offset.top &&
           py < Bottom();
  }
  bool Contains(const PhysicalOffset& point) const {
    return Contains(point.left, point.top);
  }

  bool Intersects(const PhysicalRect&) const;
  bool IntersectsInclusively(const PhysicalRect&) const;

  // Whether all edges of the rect are at full-pixel boundaries.
  // i.e.: EnclosingIntRect(this)) == this
  bool EdgesOnPixelBoundaries() const {
    return !offset.left.HasFraction() && !offset.top.HasFraction() &&
           !size.width.HasFraction() && !size.height.HasFraction();
  }

  PhysicalRect operator+(const PhysicalOffset&) const {
    return {this->offset + offset, size};
  }

  void Unite(const PhysicalRect&);
  void UniteIfNonZero(const PhysicalRect&);
  void UniteEvenIfEmpty(const PhysicalRect&);

  void Intersect(const PhysicalRect&);
  bool InclusiveIntersect(const PhysicalRect&);

  void Expand(const NGPhysicalBoxStrut&);
  void Expand(const LayoutRectOutsets&);
  void ExpandEdges(LayoutUnit top,
                   LayoutUnit right,
                   LayoutUnit bottom,
                   LayoutUnit left) {
    offset.top -= top;
    offset.left -= left;
    size.width += left + right;
    size.height += top + bottom;
  }
  void ExpandEdgesToPixelBoundaries();
  void Inflate(LayoutUnit d) { ExpandEdges(d, d, d, d); }

  void Contract(const NGPhysicalBoxStrut&);
  void Contract(const LayoutRectOutsets&);
  void ContractEdges(LayoutUnit top,
                     LayoutUnit right,
                     LayoutUnit bottom,
                     LayoutUnit left) {
    ExpandEdges(-top, -right, -bottom, -left);
  }

  void Move(const PhysicalOffset& o) { offset += o; }

  void ShiftLeftEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - X();
    SetX(edge);
    SetWidth((Width() - delta).ClampNegativeToZero());
  }
  void ShiftRightEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - Right();
    SetWidth((Width() + delta).ClampNegativeToZero());
  }
  void ShiftTopEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - Y();
    SetY(edge);
    SetHeight((Height() - delta).ClampNegativeToZero());
  }
  void ShiftBottomEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - Bottom();
    SetHeight((Height() + delta).ClampNegativeToZero());
  }

  // TODO(crbug.com/962299): These functions should upgraded to force correct
  // pixel snapping in a type-safe way.
  IntPoint PixelSnappedOffset() const { return RoundedIntPoint(offset); }
  int PixelSnappedWidth() const {
    return SnapSizeToPixel(size.width, offset.left);
  }
  int PixelSnappedHeight() const {
    return SnapSizeToPixel(size.height, offset.top);
  }
  IntSize PixelSnappedSize() const {
    return {PixelSnappedWidth(), PixelSnappedHeight()};
  }

  PhysicalOffset Center() const {
    return offset + PhysicalOffset(size.width / 2, size.height / 2);
  }

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  constexpr explicit PhysicalRect(const LayoutRect& r)
      : offset(r.X(), r.Y()), size(r.Width(), r.Height()) {}
  constexpr LayoutRect ToLayoutRect() const {
    return LayoutRect(offset.left, offset.top, size.width, size.height);
  }
  LayoutRect ToLayoutFlippedRect(const ComputedStyle&,
                                 const PhysicalSize&) const;

  constexpr explicit operator FloatRect() const {
    return FloatRect(offset.left, offset.top, size.width, size.height);
  }

  static PhysicalRect EnclosingRect(const FloatRect& rect) {
    PhysicalOffset offset(LayoutUnit::FromFloatFloor(rect.X()),
                          LayoutUnit::FromFloatFloor(rect.Y()));
    PhysicalSize size(LayoutUnit::FromFloatCeil(rect.MaxX()) - offset.left,
                      LayoutUnit::FromFloatCeil(rect.MaxY()) - offset.top);
    return PhysicalRect(offset, size);
  }

  // This is faster than EnclosingRect(). Can be used in situation that we
  // prefer performance to accuracy and haven't observed problems caused by the
  // tiny error (< LayoutUnit::Epsilon()).
  static PhysicalRect FastAndLossyFromFloatRect(const FloatRect& rect) {
    return PhysicalRect(LayoutUnit(rect.X()), LayoutUnit(rect.Y()),
                        LayoutUnit(rect.Width()), LayoutUnit(rect.Height()));
  }

  explicit PhysicalRect(const IntRect& r)
      : offset(r.Location()), size(r.Size()) {}

  static IntRect InfiniteIntRect() { return LayoutRect::InfiniteIntRect(); }

  String ToString() const;
};

inline PhysicalRect UnionRect(const PhysicalRect& a, const PhysicalRect& b) {
  auto r = a;
  r.Unite(b);
  return r;
}

inline PhysicalRect Intersection(const PhysicalRect& a, const PhysicalRect& b) {
  auto r = a;
  r.Intersect(b);
  return r;
}

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline IntRect EnclosingIntRect(const PhysicalRect& r) {
  IntPoint location = FlooredIntPoint(r.offset);
  IntPoint max_point = CeiledIntPoint(r.MaxXMaxYCorner());
  return IntRect(location, max_point - location);
}
inline IntRect PixelSnappedIntRect(const PhysicalRect& r) {
  return {r.PixelSnappedOffset(), r.PixelSnappedSize()};
}

// TODO(wangxianzhu): For temporary conversion from LayoutRect to PhysicalRect,
// where the input will be changed to PhysicalRect soon, to avoid redundant
// PhysicalRect() which can't be discovered by the compiler.
inline PhysicalRect PhysicalRectToBeNoop(const LayoutRect& r) {
  return PhysicalRect(r);
}

CORE_EXPORT PhysicalRect UnionRect(const Vector<PhysicalRect>& rects);
CORE_EXPORT PhysicalRect
UnionRectEvenIfEmpty(const Vector<PhysicalRect>& rects);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalRect&);
CORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PhysicalRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_
