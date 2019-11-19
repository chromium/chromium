/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/style/basic_shapes.h"

#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

bool BasicShapeCircle::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const BasicShapeCircle& other = To<BasicShapeCircle>(o);
  return center_x_ == other.center_x_ && center_y_ == other.center_y_ &&
         radius_ == other.radius_;
}

float BasicShapeCircle::FloatValueForRadiusInBox(FloatSize box_size) const {
  if (radius_.GetType() == BasicShapeRadius::kValue)
    return FloatValueForLength(
        radius_.Value(),
        hypotf(box_size.Width(), box_size.Height()) / sqrtf(2));

  FloatPoint center =
      FloatPointForCenterCoordinate(center_x_, center_y_, box_size);

  float width_delta = std::abs(box_size.Width() - center.X());
  float height_delta = std::abs(box_size.Height() - center.Y());
  if (radius_.GetType() == BasicShapeRadius::kClosestSide)
    return std::min(std::min(std::abs(center.X()), width_delta),
                    std::min(std::abs(center.Y()), height_delta));

  // If radius.type() == BasicShapeRadius::kFarthestSide.
  return std::max(std::max(center.X(), width_delta),
                  std::max(center.Y(), height_delta));
}

void BasicShapeCircle::GetPath(Path& path, const FloatRect& bounding_box) {
  DCHECK(path.IsEmpty());
  FloatPoint center =
      FloatPointForCenterCoordinate(center_x_, center_y_, bounding_box.Size());
  float radius = FloatValueForRadiusInBox(bounding_box.Size());
  path.AddEllipse(FloatRect(center.X() - radius + bounding_box.X(),
                            center.Y() - radius + bounding_box.Y(), radius * 2,
                            radius * 2));
}

bool BasicShapeEllipse::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const BasicShapeEllipse& other = To<BasicShapeEllipse>(o);
  return center_x_ == other.center_x_ && center_y_ == other.center_y_ &&
         radius_x_ == other.radius_x_ && radius_y_ == other.radius_y_;
}

float BasicShapeEllipse::FloatValueForRadiusInBox(
    const BasicShapeRadius& radius,
    float center,
    float box_width_or_height) const {
  if (radius.GetType() == BasicShapeRadius::kValue)
    return FloatValueForLength(radius.Value(), box_width_or_height);

  float width_or_height_delta = std::abs(box_width_or_height - center);
  if (radius.GetType() == BasicShapeRadius::kClosestSide)
    return std::min(std::abs(center), width_or_height_delta);

  DCHECK_EQ(radius.GetType(), BasicShapeRadius::kFarthestSide);
  return std::max(center, width_or_height_delta);
}

void BasicShapeEllipse::GetPath(Path& path, const FloatRect& bounding_box) {
  DCHECK(path.IsEmpty());
  FloatPoint center =
      FloatPointForCenterCoordinate(center_x_, center_y_, bounding_box.Size());
  float radius_x =
      FloatValueForRadiusInBox(radius_x_, center.X(), bounding_box.Width());
  float radius_y =
      FloatValueForRadiusInBox(radius_y_, center.Y(), bounding_box.Height());
  path.AddEllipse(FloatRect(center.X() - radius_x + bounding_box.X(),
                            center.Y() - radius_y + bounding_box.Y(),
                            radius_x * 2, radius_y * 2));
}

void BasicShapePolygon::GetPath(Path& path, const FloatRect& bounding_box) {
  DCHECK(path.IsEmpty());
  DCHECK(!(values_.size() % 2));
  wtf_size_t length = values_.size();

  if (!length)
    return;

  path.MoveTo(
      FloatPoint(FloatValueForLength(values_.at(0), bounding_box.Width()) +
                     bounding_box.X(),
                 FloatValueForLength(values_.at(1), bounding_box.Height()) +
                     bounding_box.Y()));
  for (wtf_size_t i = 2; i < length; i = i + 2) {
    path.AddLineTo(FloatPoint(
        FloatValueForLength(values_.at(i), bounding_box.Width()) +
            bounding_box.X(),
        FloatValueForLength(values_.at(i + 1), bounding_box.Height()) +
            bounding_box.Y()));
  }
  path.CloseSubpath();
}

bool BasicShapePolygon::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const BasicShapePolygon& other = To<BasicShapePolygon>(o);
  return wind_rule_ == other.wind_rule_ && values_ == other.values_;
}

void BasicShapeInset::GetPath(Path& path, const FloatRect& bounding_box) {
  DCHECK(path.IsEmpty());
  float left = FloatValueForLength(left_, bounding_box.Width());
  float top = FloatValueForLength(top_, bounding_box.Height());
  FloatRect rect(
      left + bounding_box.X(), top + bounding_box.Y(),
      std::max<float>(bounding_box.Width() - left -
                          FloatValueForLength(right_, bounding_box.Width()),
                      0),
      std::max<float>(bounding_box.Height() - top -
                          FloatValueForLength(bottom_, bounding_box.Height()),
                      0));
  const FloatSize& box_size = bounding_box.Size();
  auto radii = FloatRoundedRect::Radii(
      FloatSizeForLengthSize(top_left_radius_, box_size),
      FloatSizeForLengthSize(top_right_radius_, box_size),
      FloatSizeForLengthSize(bottom_left_radius_, box_size),
      FloatSizeForLengthSize(bottom_right_radius_, box_size));

  FloatRoundedRect final_rect(rect, radii);
  final_rect.ConstrainRadii();
  path.AddRoundedRect(final_rect);
}

bool BasicShapeInset::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const BasicShapeInset& other = To<BasicShapeInset>(o);
  return right_ == other.right_ && top_ == other.top_ &&
         bottom_ == other.bottom_ && left_ == other.left_ &&
         top_left_radius_ == other.top_left_radius_ &&
         top_right_radius_ == other.top_right_radius_ &&
         bottom_right_radius_ == other.bottom_right_radius_ &&
         bottom_left_radius_ == other.bottom_left_radius_;
}

}  // namespace blink
