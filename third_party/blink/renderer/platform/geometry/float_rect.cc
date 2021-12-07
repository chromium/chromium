/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_rect.h"

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

FloatRect FloatRect::NarrowPrecision(double x,
                                     double y,
                                     double width,
                                     double height) {
  return FloatRect(ClampTo<float>(x), ClampTo<float>(y), ClampTo<float>(width),
                   ClampTo<float>(height));
}

#if DCHECK_IS_ON()
bool FloatRect::MayNotHaveExactIntRectRepresentation() const {
  static const float kMaxExactlyExpressible = 1 << FLT_MANT_DIG;
  return fabs(x()) > kMaxExactlyExpressible ||
         fabs(y()) > kMaxExactlyExpressible ||
         fabs(width()) > kMaxExactlyExpressible ||
         fabs(height()) > kMaxExactlyExpressible ||
         fabs(right()) > kMaxExactlyExpressible ||
         fabs(bottom()) > kMaxExactlyExpressible;
}

bool FloatRect::EqualWithinEpsilon(const FloatRect& other,
                                   float epsilon) const {
  return std::abs(other.x() - x()) <= epsilon &&
         std::abs(other.y() - y()) <= epsilon &&
         std::abs(other.width() - width()) <= epsilon &&
         std::abs(other.height() - height()) <= epsilon;
}

#endif

bool FloatRect::IsFinite() const {
  return static_cast<SkRect>(*this).isFinite();
}

bool FloatRect::IsExpressibleAsIntRect() const {
  return IsWithinIntRange(x()) && IsWithinIntRange(y()) &&
         IsWithinIntRange(width()) && IsWithinIntRange(height()) &&
         IsWithinIntRange(right()) && IsWithinIntRange(bottom());
}

void FloatRect::ShiftXEdgeTo(float edge) {
  float delta = edge - x();
  set_x(edge);
  set_width(std::max(0.0f, width() - delta));
}

void FloatRect::ShiftMaxXEdgeTo(float edge) {
  float delta = edge - right();
  set_width(std::max(0.0f, width() + delta));
}

void FloatRect::ShiftYEdgeTo(float edge) {
  float delta = edge - y();
  set_y(edge);
  set_height(std::max(0.0f, height() - delta));
}

void FloatRect::ShiftMaxYEdgeTo(float edge) {
  float delta = edge - bottom();
  set_height(std::max(0.0f, height() + delta));
}

bool FloatRect::Intersects(const FloatRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && x() < other.right() &&
         other.x() < right() && y() < other.bottom() && other.y() < bottom();
}

bool FloatRect::Intersects(const gfx::Rect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && x() < other.right() &&
         other.x() < right() && y() < other.bottom() && other.y() < bottom();
}

bool FloatRect::Contains(const gfx::Rect& other) const {
  return x() <= other.x() && right() >= other.right() && y() <= other.y() &&
         bottom() >= other.bottom();
}

bool FloatRect::Contains(const FloatRect& other) const {
  return x() <= other.x() && right() >= other.right() && y() <= other.y() &&
         bottom() >= other.bottom();
}

void FloatRect::Intersect(const gfx::Rect& other) {
  float new_left = std::max(x(), static_cast<float>(other.x()));
  float new_top = std::max(y(), static_cast<float>(other.y()));
  float new_right = std::min(right(), static_cast<float>(other.right()));
  float new_bottom = std::min(bottom(), static_cast<float>(other.bottom()));

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_left >= new_right || new_top >= new_bottom) {
    new_left = 0;
    new_top = 0;
    new_right = 0;
    new_bottom = 0;
  }

  SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
}

void FloatRect::Intersect(const FloatRect& other) {
  float new_left = std::max(x(), other.x());
  float new_top = std::max(y(), other.y());
  float new_right = std::min(right(), other.right());
  float new_bottom = std::min(bottom(), other.bottom());

  // Return a clean empty rectangle for non-intersecting cases.
  if (new_left >= new_right || new_top >= new_bottom) {
    new_left = 0;
    new_top = 0;
    new_right = 0;
    new_bottom = 0;
  }

  SetLocationAndSizeFromEdges(new_left, new_top, new_right, new_bottom);
}

void FloatRect::Union(const FloatRect& other) {
  // Handle empty special cases first.
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  UnionEvenIfEmpty(other);
}

void FloatRect::UnionEvenIfEmpty(const FloatRect& other) {
  float min_x = std::min(x(), other.x());
  float min_y = std::min(y(), other.y());
  float max_x = std::max(this->right(), other.right());
  float max_y = std::max(this->bottom(), other.bottom());

  SetLocationAndSizeFromEdges(min_x, min_y, max_x, max_y);
}

void FloatRect::UnionIfNonZero(const FloatRect& other) {
  // Handle empty special cases first.
  if (other.IsZero())
    return;
  if (IsZero()) {
    *this = other;
    return;
  }

  UnionEvenIfEmpty(other);
}

void FloatRect::Extend(const gfx::PointF& p) {
  float min_x = std::min(x(), p.x());
  float min_y = std::min(y(), p.y());
  float max_x = std::max(this->right(), p.x());
  float max_y = std::max(this->bottom(), p.y());

  SetLocationAndSizeFromEdges(min_x, min_y, max_x, max_y);
}

void FloatRect::Scale(float sx, float sy) {
  location_.set_x(x() * sx);
  location_.set_y(y() * sy);
  size_.set_width(width() * sx);
  size_.set_height(height() * sy);
}

float FloatRect::SquaredDistanceTo(const gfx::PointF& point) const {
  gfx::PointF closest_point;
  closest_point.set_x(ClampTo<float>(point.x(), x(), right()));
  closest_point.set_y(ClampTo<float>(point.y(), y(), bottom()));
  return (point - closest_point).LengthSquared();
}

FloatRect UnionRects(const Vector<FloatRect>& rects) {
  FloatRect result;

  for (const auto& rect : rects)
    result.Union(rect);

  return result;
}

gfx::Rect ToEnclosedRect(const FloatRect& rect) {
  gfx::Point location = gfx::ToCeiledPoint(rect.origin());
  gfx::Point max_point = gfx::ToFlooredPoint(rect.bottom_right());
  gfx::Rect r;
  r.SetByBounds(location.x(), location.y(), max_point.x(), max_point.y());
  return r;
}

gfx::Rect RoundedIntRect(const FloatRect& rect) {
  return gfx::Rect(gfx::ToRoundedPoint(rect.origin()),
                   ToRoundedSize(rect.size()));
}

FloatRect MapRect(const FloatRect& r,
                  const FloatRect& src_rect,
                  const FloatRect& dest_rect) {
  if (!src_rect.width() || !src_rect.height())
    return FloatRect();

  float width_scale = dest_rect.width() / src_rect.width();
  float height_scale = dest_rect.height() / src_rect.height();
  return FloatRect(dest_rect.x() + (r.x() - src_rect.x()) * width_scale,
                   dest_rect.y() + (r.y() - src_rect.y()) * height_scale,
                   r.width() * width_scale, r.height() * height_scale);
}

std::ostream& operator<<(std::ostream& ostream, const FloatRect& rect) {
  return ostream << rect.ToString();
}

String FloatRect::ToString() const {
  return String::Format("%s %s", origin().ToString().c_str(),
                        size().ToString().Ascii().c_str());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const FloatRect& r) {
  ts << "at (" << WTF::TextStream::FormatNumberRespectingIntegers(r.x());
  ts << "," << WTF::TextStream::FormatNumberRespectingIntegers(r.y());
  ts << ") size " << WTF::TextStream::FormatNumberRespectingIntegers(r.width());
  ts << "x" << WTF::TextStream::FormatNumberRespectingIntegers(r.height());
  return ts;
}

}  // namespace blink
