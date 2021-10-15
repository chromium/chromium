/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/int_rect.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <algorithm>

namespace blink {

void IntRect::ShiftXEdgeTo(int edge) {
  int delta = edge - x();
  set_x(edge);
  set_width(std::max(0, width() - delta));
}

void IntRect::ShiftMaxXEdgeTo(int edge) {
  int delta = edge - right();
  set_width(std::max(0, width() + delta));
}

void IntRect::ShiftYEdgeTo(int edge) {
  int delta = edge - y();
  set_y(edge);
  set_height(std::max(0, height() - delta));
}

void IntRect::ShiftMaxYEdgeTo(int edge) {
  int delta = edge - bottom();
  set_height(std::max(0, height() + delta));
}

bool IntRect::Intersects(const IntRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && x() < other.right() &&
         other.x() < right() && y() < other.bottom() && other.y() < bottom();
}

bool IntRect::Contains(const IntRect& other) const {
  return x() <= other.x() && right() >= other.right() && y() <= other.y() &&
         bottom() >= other.bottom();
}

void IntRect::Intersect(const IntRect& other) {
  int new_left = std::max(x(), other.x());
  int new_top = std::max(y(), other.y());
  int new_right = std::min(right(), other.right());
  int new_bottom = std::min(bottom(), other.bottom());

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_left >= new_right || new_top >= new_bottom) {
    new_left = 0;
    new_top = 0;
    new_right = 0;
    new_bottom = 0;
  }

  SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
}

bool IntRect::InclusiveIntersect(const IntRect& other) {
  int new_left = std::max(x(), other.x());
  int new_top = std::max(y(), other.y());
  int new_right = std::min(right(), other.right());
  int new_bottom = std::min(bottom(), other.bottom());

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_left > new_right || new_top > new_bottom) {
    new_left = 0;
    new_top = 0;
    new_right = 0;
    new_bottom = 0;
    SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
    return false;
  }

  SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
  return true;
}

void IntRect::Union(const IntRect& other) {
  // Handle empty special cases first.
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  UnionEvenIfEmpty(other);
}

void IntRect::UnionIfNonZero(const IntRect& other) {
  // Handle empty special cases first.
  if (!other.width() && !other.height())
    return;
  if (!width() && !height()) {
    *this = other;
    return;
  }

  UnionEvenIfEmpty(other);
}

void IntRect::UnionEvenIfEmpty(const IntRect& other) {
  int new_left = std::min(x(), other.x());
  int new_top = std::min(y(), other.y());
  int new_right = std::max(right(), other.right());
  int new_bottom = std::max(bottom(), other.bottom());

  SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
}

void IntRect::Scale(float s) {
  location_.set_x((int)(x() * s));
  location_.set_y((int)(y() * s));
  size_.set_width((int)(width() * s));
  size_.set_height((int)(height() * s));
}

static inline int DistanceToInterval(int pos, int start, int end) {
  if (pos < start)
    return start - pos;
  if (pos > end)
    return end - pos;
  return 0;
}

IntSize IntRect::DifferenceToPoint(const IntPoint& point) const {
  int xdistance = DistanceToInterval(point.x(), x(), right());
  int ydistance = DistanceToInterval(point.y(), y(), bottom());
  return IntSize(xdistance, ydistance);
}

IntRect UnionRects(const Vector<IntRect>& rects) {
  IntRect result;

  for (const IntRect& rect : rects)
    result.Union(rect);

  return result;
}

IntRect UnionRectsEvenIfEmpty(const Vector<IntRect>& rects) {
  wtf_size_t count = rects.size();
  if (!count)
    return IntRect();

  IntRect result = rects[0];
  for (wtf_size_t i = 1; i < count; ++i)
    result.UnionEvenIfEmpty(rects[i]);

  return result;
}

IntRect MaximumCoveredRect(const IntRect& a, const IntRect& b) {
  // Check a or b by itself.
  IntRect maximum(a);
  auto maximum_area = a.size().Area();
  if (b.size().Area() > maximum_area) {
    maximum = b;
    maximum_area = b.size().Area();
  }
  // Check the regions that include the intersection of a and b. This can be
  // done by taking the intersection and expanding it vertically and
  // horizontally. These expanded intersections will both still be covered by
  // a or b.
  IntRect intersection = a;
  intersection.InclusiveIntersect(b);
  if (!intersection.size().IsZero()) {
    IntRect vert_expanded_intersection(intersection);
    vert_expanded_intersection.ShiftYEdgeTo(std::min(a.y(), b.y()));
    vert_expanded_intersection.ShiftMaxYEdgeTo(
        std::max(a.bottom(), b.bottom()));
    if (vert_expanded_intersection.size().Area() > maximum_area) {
      maximum = vert_expanded_intersection;
      maximum_area = vert_expanded_intersection.size().Area();
    }
    IntRect horiz_expanded_intersection(intersection);
    horiz_expanded_intersection.ShiftXEdgeTo(std::min(a.x(), b.x()));
    horiz_expanded_intersection.ShiftMaxXEdgeTo(std::max(a.right(), b.right()));
    if (horiz_expanded_intersection.size().Area() > maximum_area) {
      maximum = horiz_expanded_intersection;
      maximum_area = horiz_expanded_intersection.size().Area();
    }
  }
  return maximum;
}

std::ostream& operator<<(std::ostream& ostream, const IntRect& rect) {
  return ostream << rect.ToString();
}

String IntRect::ToString() const {
  return String::Format("%s %s", origin().ToString().Ascii().c_str(),
                        size().ToString().Ascii().c_str());
}

bool IntRect::IsValid() const {
  base::CheckedNumeric<int> max = location_.x();
  max += size_.width();
  if (!max.IsValid())
    return false;
  max = location_.y();
  max += size_.height();
  return max.IsValid();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const IntRect& r) {
  return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x"
            << r.height();
}

}  // namespace blink
