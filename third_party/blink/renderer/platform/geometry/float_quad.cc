/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Xidorn Quan (quanxunzhen@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_quad.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

static inline float Min4(float a, float b, float c, float d) {
  return std::min(std::min(a, b), std::min(c, d));
}

static inline float Max4(float a, float b, float c, float d) {
  return std::max(std::max(a, b), std::max(c, d));
}

inline bool IsPointInTriangle(const gfx::PointF& p,
                              const gfx::PointF& t1,
                              const gfx::PointF& t2,
                              const gfx::PointF& t3) {
  // Compute vectors
  gfx::Vector2dF v0 = t3 - t1;
  gfx::Vector2dF v1 = t2 - t1;
  gfx::Vector2dF v2 = p - t1;

  // Compute dot products
  double dot00 = gfx::DotProduct(v0, v0);
  double dot01 = gfx::DotProduct(v0, v1);
  double dot02 = gfx::DotProduct(v0, v2);
  double dot11 = gfx::DotProduct(v1, v1);
  double dot12 = gfx::DotProduct(v1, v2);

  // Compute barycentric coordinates
  double inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
  double u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
  double v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

  // Check if point is in triangle
  return (u >= 0) && (v >= 0) && (u + v <= 1);
}

static inline float ClampToIntRange(float value) {
  if (UNLIKELY(std::isinf(value) ||
               std::abs(value) > std::numeric_limits<int>::max())) {
    return std::signbit(value) ? std::numeric_limits<int>::min()
                               : std::numeric_limits<int>::max();
  }
  return value;
}

FloatRect FloatQuad::BoundingBox() const {
  float left = ClampToIntRange(Min4(p1_.x(), p2_.x(), p3_.x(), p4_.x()));
  float top = ClampToIntRange(Min4(p1_.y(), p2_.y(), p3_.y(), p4_.y()));

  float right = ClampToIntRange(Max4(p1_.x(), p2_.x(), p3_.x(), p4_.x()));
  float bottom = ClampToIntRange(Max4(p1_.y(), p2_.y(), p3_.y(), p4_.y()));

  return FloatRect(left, top, right - left, bottom - top);
}

static inline bool WithinEpsilon(float a, float b) {
  return fabs(a - b) < std::numeric_limits<float>::epsilon();
}

FloatQuad::FloatQuad(const SkPoint (&quad)[4])
    : FloatQuad(gfx::SkPointToPointF(quad[0]),
                gfx::SkPointToPointF(quad[1]),
                gfx::SkPointToPointF(quad[2]),
                gfx::SkPointToPointF(quad[3])) {}

bool FloatQuad::IsRectilinear() const {
  return (WithinEpsilon(p1_.x(), p2_.x()) && WithinEpsilon(p2_.y(), p3_.y()) &&
          WithinEpsilon(p3_.x(), p4_.x()) && WithinEpsilon(p4_.y(), p1_.y())) ||
         (WithinEpsilon(p1_.y(), p2_.y()) && WithinEpsilon(p2_.x(), p3_.x()) &&
          WithinEpsilon(p3_.y(), p4_.y()) && WithinEpsilon(p4_.x(), p1_.x()));
}

bool FloatQuad::ContainsPoint(const gfx::PointF& p) const {
  return IsPointInTriangle(p, p1_, p2_, p3_) ||
         IsPointInTriangle(p, p1_, p3_, p4_);
}

// Note that we only handle convex quads here.
bool FloatQuad::ContainsQuad(const FloatQuad& other) const {
  return ContainsPoint(other.p1()) && ContainsPoint(other.p2()) &&
         ContainsPoint(other.p3()) && ContainsPoint(other.p4());
}

static inline gfx::PointF RightMostCornerToVector(
    const FloatRect& rect,
    const gfx::Vector2dF& vector) {
  // Return the corner of the rectangle that if it is to the left of the vector
  // would mean all of the rectangle is to the left of the vector.
  // The vector here represents the side between two points in a clockwise
  // convex polygon.
  //
  //  Q  XXX
  // QQQ XXX   If the lower left corner of X is left of the vector that goes
  //  QQQ      from the top corner of Q to the right corner of Q, then all of X
  //   Q       is left of the vector, and intersection impossible.
  //
  gfx::PointF point;
  if (vector.x() >= 0)
    point.set_y(rect.bottom());
  else
    point.set_y(rect.y());
  if (vector.y() >= 0)
    point.set_x(rect.x());
  else
    point.set_x(rect.right());
  return point;
}

bool FloatQuad::IntersectsRect(const FloatRect& rect) const {
  // IntersectsRect is only valid on convex quads which an empty quad is not.
  DCHECK(!IsEmpty());

  // For each side of the quad clockwise we check if the rectangle is to the
  // left of it since only content on the right can onlap with the quad.  This
  // only works if the quad is convex.
  gfx::Vector2dF v1, v2, v3, v4;

  // Ensure we use clockwise vectors.
  if (!IsCounterclockwise()) {
    v1 = p2_ - p1_;
    v2 = p3_ - p2_;
    v3 = p4_ - p3_;
    v4 = p1_ - p4_;
  } else {
    v1 = p4_ - p1_;
    v2 = p1_ - p2_;
    v3 = p2_ - p3_;
    v4 = p3_ - p4_;
  }

  gfx::PointF p = RightMostCornerToVector(rect, v1);
  if (gfx::CrossProduct(v1, p - p1_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v2);
  if (gfx::CrossProduct(v2, p - p2_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v3);
  if (gfx::CrossProduct(v3, p - p3_) < 0)
    return false;

  p = RightMostCornerToVector(rect, v4);
  if (gfx::CrossProduct(v4, p - p4_) < 0)
    return false;

  // If not all of the rectangle is outside one of the quad's four sides, then
  // that means at least a part of the rectangle is overlapping the quad.
  return true;
}

// Tests whether the line is contained by or intersected with the circle.
static inline bool LineIntersectsCircle(const gfx::PointF& center,
                                        float radius,
                                        const gfx::PointF& p0,
                                        const gfx::PointF& p1) {
  float x0 = p0.x() - center.x(), y0 = p0.y() - center.y();
  float x1 = p1.x() - center.x(), y1 = p1.y() - center.y();
  float radius2 = radius * radius;
  if ((x0 * x0 + y0 * y0) <= radius2 || (x1 * x1 + y1 * y1) <= radius2)
    return true;
  if (p0 == p1)
    return false;

  float a = y0 - y1;
  float b = x1 - x0;
  float c = x0 * y1 - x1 * y0;
  float distance2 = c * c / (a * a + b * b);
  // If distance between the center point and the line > the radius,
  // the line doesn't cross (or is contained by) the ellipse.
  if (distance2 > radius2)
    return false;

  // The nearest point on the line is between p0 and p1?
  float x = -a * c / (a * a + b * b);
  float y = -b * c / (a * a + b * b);

  return (((x0 <= x && x <= x1) || (x0 >= x && x >= x1)) &&
          ((y0 <= y && y <= y1) || (y1 <= y && y <= y0)));
}

bool FloatQuad::IntersectsCircle(const gfx::PointF& center,
                                 float radius) const {
  return ContainsPoint(
             center)  // The circle may be totally contained by the quad.
         || LineIntersectsCircle(center, radius, p1_, p2_) ||
         LineIntersectsCircle(center, radius, p2_, p3_) ||
         LineIntersectsCircle(center, radius, p3_, p4_) ||
         LineIntersectsCircle(center, radius, p4_, p1_);
}

bool FloatQuad::IntersectsEllipse(const gfx::PointF& center,
                                  const gfx::SizeF& radii) const {
  // Transform the ellipse to an origin-centered circle whose radius is the
  // product of major radius and minor radius.  Here we apply the same
  // transformation to the quad.
  FloatQuad transformed_quad(*this);
  transformed_quad.Move(-center.x(), -center.y());
  transformed_quad.Scale(radii.height(), radii.width());

  gfx::PointF origin_point;
  return transformed_quad.IntersectsCircle(origin_point,
                                           radii.height() * radii.width());
}

bool FloatQuad::IsCounterclockwise() const {
  // Return if the two first vectors are turning clockwise. If the quad is
  // convex then all following vectors will turn the same way.
  return gfx::CrossProduct(p2_ - p1_, p3_ - p2_) < 0;
}

std::ostream& operator<<(std::ostream& ostream, const FloatQuad& quad) {
  return ostream << quad.ToString();
}

String FloatQuad::ToString() const {
  return String::Format("%s; %s; %s; %s", p1_.ToString().c_str(),
                        p2_.ToString().c_str(), p3_.ToString().c_str(),
                        p4_.ToString().c_str());
}

}  // namespace blink
