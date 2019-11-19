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

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FloatRect FloatRect::NarrowPrecision(double x,
                                     double y,
                                     double width,
                                     double height) {
  return FloatRect(clampTo<float>(x), clampTo<float>(y), clampTo<float>(width),
                   clampTo<float>(height));
}

#if DCHECK_IS_ON()
bool FloatRect::MayNotHaveExactIntRectRepresentation() const {
  static const float kMaxExactlyExpressible = 1 << FLT_MANT_DIG;
  return fabs(X()) > kMaxExactlyExpressible ||
         fabs(Y()) > kMaxExactlyExpressible ||
         fabs(Width()) > kMaxExactlyExpressible ||
         fabs(Height()) > kMaxExactlyExpressible ||
         fabs(MaxX()) > kMaxExactlyExpressible ||
         fabs(MaxY()) > kMaxExactlyExpressible;
}

bool FloatRect::EqualWithinEpsilon(const FloatRect& other,
                                   float epsilon) const {
  return std::abs(other.X() - X()) <= epsilon &&
         std::abs(other.Y() - Y()) <= epsilon &&
         std::abs(other.Width() - Width()) <= epsilon &&
         std::abs(other.Height() - Height()) <= epsilon;
}

#endif

bool FloatRect::IsExpressibleAsIntRect() const {
  return isWithinIntRange(X()) && isWithinIntRange(Y()) &&
         isWithinIntRange(Width()) && isWithinIntRange(Height()) &&
         isWithinIntRange(MaxX()) && isWithinIntRange(MaxY());
}

void FloatRect::ShiftXEdgeTo(float edge) {
  float delta = edge - X();
  SetX(edge);
  SetWidth(std::max(0.0f, Width() - delta));
}

void FloatRect::ShiftMaxXEdgeTo(float edge) {
  float delta = edge - MaxX();
  SetWidth(std::max(0.0f, Width() + delta));
}

void FloatRect::ShiftYEdgeTo(float edge) {
  float delta = edge - Y();
  SetY(edge);
  SetHeight(std::max(0.0f, Height() - delta));
}

void FloatRect::ShiftMaxYEdgeTo(float edge) {
  float delta = edge - MaxY();
  SetHeight(std::max(0.0f, Height() + delta));
}

bool FloatRect::Intersects(const FloatRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && X() < other.MaxX() &&
         other.X() < MaxX() && Y() < other.MaxY() && other.Y() < MaxY();
}

bool FloatRect::Intersects(const IntRect& other) const {
  // Checking emptiness handles negative widths as well as zero.
  return !IsEmpty() && !other.IsEmpty() && X() < other.MaxX() &&
         other.X() < MaxX() && Y() < other.MaxY() && other.Y() < MaxY();
}

bool FloatRect::Contains(const IntRect& other) const {
  return X() <= other.X() && MaxX() >= other.MaxX() && Y() <= other.Y() &&
         MaxY() >= other.MaxY();
}

bool FloatRect::Contains(const FloatRect& other) const {
  return X() <= other.X() && MaxX() >= other.MaxX() && Y() <= other.Y() &&
         MaxY() >= other.MaxY();
}

bool FloatRect::Contains(const FloatPoint& point,
                         ContainsMode contains_mode) const {
  if (contains_mode == kInsideOrOnStroke)
    return Contains(point.X(), point.Y());
  return X() < point.X() && MaxX() > point.X() && Y() < point.Y() &&
         MaxY() > point.Y();
}

void FloatRect::Intersect(const IntRect& other) {
  float left = std::max(X(), static_cast<float>(other.X()));
  float top = std::max(Y(), static_cast<float>(other.Y()));
  float right = std::min(MaxX(), static_cast<float>(other.MaxX()));
  float bottom = std::min(MaxY(), static_cast<float>(other.MaxY()));

  // Return a clean empty rectangle for non-intersecting cases.
  if (left >= right || top >= bottom) {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
  }

  SetLocationAndSizeFromEdges(left, top, right, bottom);
}

void FloatRect::Intersect(const FloatRect& other) {
  float left = std::max(X(), other.X());
  float top = std::max(Y(), other.Y());
  float right = std::min(MaxX(), other.MaxX());
  float bottom = std::min(MaxY(), other.MaxY());

  // Return a clean empty rectangle for non-intersecting cases.
  if (left >= right || top >= bottom) {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
  }

  SetLocationAndSizeFromEdges(left, top, right, bottom);
}

bool FloatRect::InclusiveIntersect(const FloatRect& other) {
  float left = std::max(X(), other.X());
  float top = std::max(Y(), other.Y());
  float right = std::min(MaxX(), other.MaxX());
  float bottom = std::min(MaxY(), other.MaxY());

  // Return a clean empty rectangle for non-intersecting cases.
  if (left > right || top > bottom) {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
    SetLocationAndSizeFromEdges(left, top, right, bottom);
    return false;
  }

  SetLocationAndSizeFromEdges(left, top, right, bottom);
  return true;
}

void FloatRect::Unite(const FloatRect& other) {
  // Handle empty special cases first.
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void FloatRect::UniteEvenIfEmpty(const FloatRect& other) {
  float min_x = std::min(X(), other.X());
  float min_y = std::min(Y(), other.Y());
  float max_x = std::max(this->MaxX(), other.MaxX());
  float max_y = std::max(this->MaxY(), other.MaxY());

  SetLocationAndSizeFromEdges(min_x, min_y, max_x, max_y);
}

void FloatRect::UniteIfNonZero(const FloatRect& other) {
  // Handle empty special cases first.
  if (other.IsZero())
    return;
  if (IsZero()) {
    *this = other;
    return;
  }

  UniteEvenIfEmpty(other);
}

void FloatRect::Extend(const FloatPoint& p) {
  float min_x = std::min(X(), p.X());
  float min_y = std::min(Y(), p.Y());
  float max_x = std::max(this->MaxX(), p.X());
  float max_y = std::max(this->MaxY(), p.Y());

  SetLocationAndSizeFromEdges(min_x, min_y, max_x, max_y);
}

void FloatRect::Scale(float sx, float sy) {
  location_.SetX(X() * sx);
  location_.SetY(Y() * sy);
  size_.SetWidth(Width() * sx);
  size_.SetHeight(Height() * sy);
}

float FloatRect::SquaredDistanceTo(const FloatPoint& point) const {
  FloatPoint closest_point;
  closest_point.SetX(clampTo<float>(point.X(), X(), MaxX()));
  closest_point.SetY(clampTo<float>(point.Y(), Y(), MaxY()));
  return (point - closest_point).DiagonalLengthSquared();
}

FloatRect UnionRect(const Vector<FloatRect>& rects) {
  FloatRect result;

  for (const auto& rect : rects)
    result.Unite(rect);

  return result;
}

IntRect EnclosedIntRect(const FloatRect& rect) {
  IntPoint location = CeiledIntPoint(rect.Location());
  IntPoint max_point = FlooredIntPoint(rect.MaxXMaxYCorner());
  IntSize size(base::ClampSub(max_point.X(), location.X()),
               base::ClampSub(max_point.Y(), location.Y()));
  size.ClampNegativeToZero();
  return IntRect(location, size);
}

IntRect RoundedIntRect(const FloatRect& rect) {
  return IntRect(RoundedIntPoint(rect.Location()), RoundedIntSize(rect.Size()));
}

FloatRect MapRect(const FloatRect& r,
                  const FloatRect& src_rect,
                  const FloatRect& dest_rect) {
  if (!src_rect.Width() || !src_rect.Height())
    return FloatRect();

  float width_scale = dest_rect.Width() / src_rect.Width();
  float height_scale = dest_rect.Height() / src_rect.Height();
  return FloatRect(dest_rect.X() + (r.X() - src_rect.X()) * width_scale,
                   dest_rect.Y() + (r.Y() - src_rect.Y()) * height_scale,
                   r.Width() * width_scale, r.Height() * height_scale);
}

std::ostream& operator<<(std::ostream& ostream, const FloatRect& rect) {
  return ostream << rect.ToString();
}

String FloatRect::ToString() const {
  return String::Format("%s %s", Location().ToString().Ascii().c_str(),
                        Size().ToString().Ascii().c_str());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const FloatRect& r) {
  ts << "at (" << WTF::TextStream::FormatNumberRespectingIntegers(r.X());
  ts << "," << WTF::TextStream::FormatNumberRespectingIntegers(r.Y());
  ts << ") size " << WTF::TextStream::FormatNumberRespectingIntegers(r.Width());
  ts << "x" << WTF::TextStream::FormatNumberRespectingIntegers(r.Height());
  return ts;
}

}  // namespace blink
