// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LogicalRect PhysicalRect::ConvertToLogical(WritingMode mode,
                                           TextDirection direction,
                                           PhysicalSize outer_size,
                                           PhysicalSize inner_size) const {
  return LogicalRect(
      offset.ConvertToLogical(mode, direction, outer_size, inner_size),
      size.ConvertToLogical(mode));
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
  offset = {left, top};
  size = {right - left, bottom - top};
}

void PhysicalRect::Expand(const NGPhysicalBoxStrut& strut) {
  ExpandEdges(strut.top, strut.right, strut.bottom, strut.left);
}

void PhysicalRect::Expand(const LayoutRectOutsets& outsets) {
  ExpandEdges(outsets.Top(), outsets.Right(), outsets.Bottom(), outsets.Left());
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

void PhysicalRect::Contract(const NGPhysicalBoxStrut& strut) {
  ExpandEdges(-strut.top, -strut.right, -strut.bottom, -strut.left);
}

void PhysicalRect::Contract(const LayoutRectOutsets& outsets) {
  ExpandEdges(-outsets.Top(), -outsets.Right(), -outsets.Bottom(),
              -outsets.Left());
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

LayoutRect PhysicalRect::ToLayoutFlippedRect(
    const ComputedStyle& style,
    const PhysicalSize& container_size) const {
  if (!style.IsFlippedBlocksWritingMode())
    return {offset.left, offset.top, size.width, size.height};
  return {container_size.width - offset.left - size.width, offset.top,
          size.width, size.height};
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
