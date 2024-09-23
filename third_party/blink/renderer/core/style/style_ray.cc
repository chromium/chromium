// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_ray.h"

#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

scoped_refptr<StyleRay> StyleRay::Create(
    float angle,
    RaySize size,
    bool contain,
    const BasicShapeCenterCoordinate& center_x,
    const BasicShapeCenterCoordinate& center_y,
    bool has_explicit_center) {
  return base::AdoptRef(new StyleRay(angle, size, contain, center_x, center_y,
                                     has_explicit_center));
}

StyleRay::StyleRay(float angle,
                   RaySize size,
                   bool contain,
                   const BasicShapeCenterCoordinate& center_x,
                   const BasicShapeCenterCoordinate& center_y,
                   bool has_explicit_center)
    : angle_(angle),
      size_(size),
      contain_(contain),
      center_x_(center_x),
      center_y_(center_y),
      has_explicit_center_(has_explicit_center) {}

bool StyleRay::IsEqualAssumingSameType(const BasicShape& o) const {
  const StyleRay& other = To<StyleRay>(o);
  return angle_ == other.angle_ && size_ == other.size_ &&
         contain_ == other.contain_ && center_x_ == other.center_x_ &&
         center_y_ == other.center_y_ &&
         has_explicit_center_ == other.has_explicit_center_;
}

void StyleRay::GetPath(Path&, const gfx::RectF&, float) const {
  // ComputedStyle::ApplyMotionPathTransform cannot call GetPath
  // for rays as they may have infinite length.
  NOTREACHED_IN_MIGRATION();
}

namespace {

float CalculatePerpendicularDistanceToReferenceBoxSide(
    const gfx::PointF& point,
    const gfx::SizeF& reference_box_size,
    float (*comp)(std::initializer_list<float>)) {
  return comp(
      {std::abs(point.x()), std::abs(point.x() - reference_box_size.width()),
       std::abs(point.y()), std::abs(point.y() - reference_box_size.height())});
}

float CalculateDistance(const gfx::PointF& a, const gfx::PointF& b) {
  return (a - b).Length();
}

float CalculateDistanceToReferenceBoxCorner(
    const gfx::PointF& point,
    const gfx::SizeF& box_size,
    float (*comp)(std::initializer_list<float>)) {
  return comp({CalculateDistance(point, {0, 0}),
               CalculateDistance(point, {box_size.width(), 0}),
               CalculateDistance(point, {box_size.width(), box_size.height()}),
               CalculateDistance(point, {0, box_size.height()})});
}

float CalculateDistanceToReferenceBoxSide(
    const gfx::PointF& point,
    const float angle,
    const gfx::SizeF& reference_box_size) {
  if (!gfx::RectF(reference_box_size).InclusiveContains(point)) {
    return 0;
  }
  const float theta = Deg2rad(angle);
  float cos_t = std::cos(theta);
  float sin_t = std::sin(theta);
  // We are looking for % point, let's swap signs and lines
  // so that we end up in situation like this:
  //         (0, 0) #--------------%--# (box.width, 0)
  //                |        |    /   |
  //                |        v   /    |
  //                |        |  /     |
  //                |        |t/      |
  //                |        |/       |
  //                 (point) *---h----* (box.width, point.y)
  //                |        |        |
  //                |        |        |
  // (0, box.height)#-----------------# (box.width, box.height)

  // cos_t and sin_t swapped due to the 0 angle is pointing up.
  const float vertical =
      cos_t >= 0 ? point.y() : reference_box_size.height() - point.y();
  const float horizontal =
      sin_t >= 0 ? reference_box_size.width() - point.x() : point.x();
  cos_t = std::abs(cos_t);
  sin_t = std::abs(sin_t);
  // Check what side we hit.
  if (vertical * sin_t > horizontal * cos_t) {
    return horizontal / sin_t;
  }
  return vertical / cos_t;
}

}  // namespace

float StyleRay::CalculateRayPathLength(
    const gfx::PointF& starting_point,
    const gfx::SizeF& reference_box_size) const {
  switch (Size()) {
    case StyleRay::RaySize::kClosestSide:
      return CalculatePerpendicularDistanceToReferenceBoxSide(
          starting_point, reference_box_size, std::min);
    case StyleRay::RaySize::kFarthestSide:
      return CalculatePerpendicularDistanceToReferenceBoxSide(
          starting_point, reference_box_size, std::max);
    case StyleRay::RaySize::kClosestCorner:
      return CalculateDistanceToReferenceBoxCorner(
          starting_point, reference_box_size, std::min);
    case StyleRay::RaySize::kFarthestCorner:
      return CalculateDistanceToReferenceBoxCorner(
          starting_point, reference_box_size, std::max);
    case StyleRay::RaySize::kSides:
      return CalculateDistanceToReferenceBoxSide(starting_point, Angle(),
                                                 reference_box_size);
  }
}

PointAndTangent StyleRay::PointAndNormalAtLength(
    const gfx::PointF& starting_point,
    float length) const {
  const float angle = Angle() - 90;
  const float rad = Deg2rad(angle);
  const float x = starting_point.x() + length * std::cos(rad);
  const float y = starting_point.y() + length * std::sin(rad);
  return {{x, y}, angle};
}

}  // namespace blink
