/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

#include <stdio.h>
#include <algorithm>
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LayoutRect::LayoutRect(const DoubleRect& r)
    : location_(LayoutPoint(r.Location())), size_(LayoutSize(r.Size())) {}

bool LayoutRect::Intersects(const LayoutRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && X() < other.MaxX() &&
         other.X() < MaxX() && Y() < other.MaxY() && other.Y() < MaxY();
}

bool LayoutRect::Contains(const LayoutRect& other) const {
  return X() <= other.X() && MaxX() >= other.MaxX() && Y() <= other.Y() &&
         MaxY() >= other.MaxY();
}

void LayoutRect::Intersect(const LayoutRect& other) {
  LayoutPoint new_location(std::max(X(), other.X()), std::max(Y(), other.Y()));
  LayoutPoint new_max_point(std::min(MaxX(), other.MaxX()),
                            std::min(MaxY(), other.MaxY()));

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_location.X() >= new_max_point.X() ||
      new_location.Y() >= new_max_point.Y()) {
    new_location = LayoutPoint();
    new_max_point = LayoutPoint();
  }

  location_ = new_location;
  size_ = new_max_point - new_location;
}

bool LayoutRect::InclusiveIntersect(const LayoutRect& other) {
  LayoutPoint new_location(std::max(X(), other.X()), std::max(Y(), other.Y()));
  LayoutPoint new_max_point(std::min(MaxX(), other.MaxX()),
                            std::min(MaxY(), other.MaxY()));

  if (new_location.X() > new_max_point.X() ||
      new_location.Y() > new_max_point.Y()) {
    *this = LayoutRect();
    return false;
  }

  location_ = new_location;
  size_ = new_max_point - new_location;
  return true;
}

bool LayoutRect::IntersectsInclusively(const LayoutRect& other) {
  // TODO(pdr): How should negative widths or heights be handled?
  return X() <= other.MaxX() && other.X() <= MaxX() && Y() <= other.MaxY() &&
         other.Y() <= MaxY();
}

void LayoutRect::Unite(const LayoutRect& other) {
  // Handle empty special cases first.
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void LayoutRect::UniteIfNonZero(const LayoutRect& other) {
  // Handle empty special cases first.
  if (!other.Width() && !other.Height())
    return;
  if (!Width() && !Height()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void LayoutRect::UniteEvenIfEmpty(const LayoutRect& other) {
  LayoutPoint new_location(std::min(X(), other.X()), std::min(Y(), other.Y()));
  LayoutPoint new_max_point(std::max(MaxX(), other.MaxX()),
                            std::max(MaxY(), other.MaxY()));

  location_ = new_location;
  size_ = new_max_point - new_location;
}

void LayoutRect::Scale(float s) {
  location_.Scale(s, s);
  size_.Scale(s);
}

void LayoutRect::Scale(float x_axis_scale, float y_axis_scale) {
  location_.Scale(x_axis_scale, y_axis_scale);
  size_.Scale(x_axis_scale, y_axis_scale);
}

LayoutRect UnionRect(const Vector<LayoutRect>& rects) {
  LayoutRect result;

  for (const LayoutRect& rect : rects)
    result.Unite(rect);

  return result;
}

LayoutRect UnionRectEvenIfEmpty(const Vector<LayoutRect>& rects) {
  wtf_size_t count = rects.size();
  if (!count)
    return LayoutRect();

  LayoutRect result = rects[0];
  for (wtf_size_t i = 1; i < count; ++i)
    result.UniteEvenIfEmpty(rects[i]);

  return result;
}

std::ostream& operator<<(std::ostream& ostream, const LayoutRect& rect) {
  return ostream << rect.ToString();
}

String LayoutRect::ToString() const {
  return String::Format("%s %s", Location().ToString().Ascii().c_str(),
                        Size().ToString().Ascii().c_str());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const LayoutRect& rect) {
  return ts << FloatRect(rect);
}

}  // namespace blink
