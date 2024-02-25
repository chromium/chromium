// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PhysicalSize PhysicalRect::DistanceAsSize(PhysicalOffset target) const {
  target -= offset;
  PhysicalSize distance;
  if (target.left < 0)
    distance.width = -target.left;
  else if (target.left > size.width)
    distance.width = target.left - size.width;
  if (target.top < 0)
    distance.height = -target.top;
  else if (target.top > size.height)
    distance.height = target.top - size.height;
  return distance;
}

LayoutUnit PhysicalRect::SquaredDistanceTo(const PhysicalOffset& point) const {
  LayoutUnit x1 = X(), x2 = Right();
  if (x1 > x2)
    std::swap(x1, x2);
  LayoutUnit diff_x = point.left - ClampTo<LayoutUnit>(point.left, x1, x2);
  LayoutUnit y1 = Y(), y2 = Bottom();
  if (y1 > y2)
    std::swap(y1, y2);
  LayoutUnit diff_y = point.top - ClampTo<LayoutUnit>(point.top, y1, y2);
  return diff_x * diff_x + diff_y * diff_y;
}

bool PhysicalRect::Contains(const PhysicalRect& other) const {
  return offset.left <= other.offset.left && offset.top <= other.offset.top &&
         Right() >= other.Right() && Bottom() >= other.Bottom();
}

bool PhysicalRect::Intersects(const PhysicalRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && offset.left < other.Right() &&
         other.offset.left < Right() && offset.top < other.Bottom() &&
         other.offset.top < Bottom();
}

bool PhysicalRect::IntersectsInclusively(const PhysicalRect& other) const {
  // TODO(pdr): How should negative widths or heights be handled?
  return offset.left <= other.Right() && other.offset.left <= Right() &&
         offset.top <= other.Bottom() && other.offset.top <= Bottom();
}

void PhysicalRect::Unite(const PhysicalRect& other) {
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void PhysicalRect::UniteIfNonZero(const PhysicalRect& other) {
  if (other.size.IsZero())
    return;
  if (size.IsZero()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void PhysicalRect::UniteEvenIfEmpty(const PhysicalRect& other) {
  LayoutUnit left = std::min(offset.left, other.offset.left);
  LayoutUnit top = std::min(offset.top, other.offset.top);
  LayoutUnit right = std::max(Right(), other.Right());
  LayoutUnit bottom = std::max(Bottom(), other.Bottom());
  size = {right - left, bottom - top};

  // If either width or height are not saturated, right - width == left and
  // bottom - height == top. If they are saturated, instead of using left/top
  // directly for the offset, the subtraction results in the united rect to
  // favor content in the positive directions.
  // Note that this is just a heuristic as the true rect would normally be
  // larger than the max LayoutUnit value.
  offset = {right - size.width, bottom - size.height};
}

void PhysicalRect::Expand(const PhysicalBoxStrut& strut) {
  ExpandEdges(strut.top, strut.right, strut.bottom, strut.left);
}

void PhysicalRect::ExpandEdgesToPixelBoundaries() {
  int left = FloorToInt(offset.left);
  int top = FloorToInt(offset.top);
  int max_right = (offset.left + size.width).Ceil();
  int max_bottom = (offset.top + size.height).Ceil();
  offset.left = LayoutUnit(left);
  offset.top = LayoutUnit(top);
  size.width = LayoutUnit(max_right - left);
  size.height = LayoutUnit(max_bottom - top);
}

void PhysicalRect::Contract(const PhysicalBoxStrut& strut) {
  ExpandEdges(-strut.top, -strut.right, -strut.bottom, -strut.left);
}

void PhysicalRect::Intersect(const PhysicalRect& other) {
  PhysicalOffset new_offset(std::max(X(), other.X()), std::max(Y(), other.Y()));
  PhysicalOffset new_max_point(std::min(Right(), other.Right()),
                               std::min(Bottom(), other.Bottom()));

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_offset.left >= new_max_point.left ||
      new_offset.top >= new_max_point.top) {
    new_offset = PhysicalOffset();
    new_max_point = PhysicalOffset();
  }

  offset = new_offset;
  size = {new_max_point.left - new_offset.left,
          new_max_point.top - new_offset.top};
}

bool PhysicalRect::InclusiveIntersect(const PhysicalRect& other) {
  PhysicalOffset new_offset(std::max(X(), other.X()), std::max(Y(), other.Y()));
  PhysicalOffset new_max_point(std::min(Right(), other.Right()),
                               std::min(Bottom(), other.Bottom()));

  if (new_offset.left > new_max_point.left ||
      new_offset.top > new_max_point.top) {
    *this = PhysicalRect();
    return false;
  }

  offset = new_offset;
  size = {new_max_point.left - new_offset.left,
          new_max_point.top - new_offset.top};
  return true;
}

String PhysicalRect::ToString() const {
  return String::Format("%s %s", offset.ToString().Ascii().c_str(),
                        size.ToString().Ascii().c_str());
}

PhysicalRect UnionRect(const Vector<PhysicalRect>& rects) {
  PhysicalRect result;
  for (const auto& rect : rects)
    result.Unite(rect);
  return result;
}

PhysicalRect UnionRectEvenIfEmpty(const Vector<PhysicalRect>& rects) {
  wtf_size_t count = rects.size();
  if (!count)
    return PhysicalRect();

  PhysicalRect result = rects[0];
  for (wtf_size_t i = 1; i < count; ++i)
    result.UniteEvenIfEmpty(rects[i]);

  return result;
}

std::ostream& operator<<(std::ostream& os, const PhysicalRect& value) {
  return os << value.ToString();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PhysicalRect& r) {
  // This format is required by some layout tests.
  ts << "at ("
     << WTF::TextStream::FormatNumberRespectingIntegers(r.X().ToFloat());
  ts << "," << WTF::TextStream::FormatNumberRespectingIntegers(r.Y().ToFloat());
  ts << ") size "
     << WTF::TextStream::FormatNumberRespectingIntegers(r.Width().ToFloat());
  ts << "x"
     << WTF::TextStream::FormatNumberRespectingIntegers(r.Height().ToFloat());
  return ts;
}

}  // namespace blink
