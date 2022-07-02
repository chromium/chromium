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
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

bool BasicShapeCircle::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapeCircle& other = To<BasicShapeCircle>(o);
  return center_x_ == other.center_x_ && center_y_ == other.center_y_ &&
         radius_ == other.radius_;
}

float BasicShapeCircle::FloatValueForRadiusInBox(gfx::SizeF box_size) const {
  if (radius_.GetType() == BasicShapeRadius::kValue) {
    return FloatValueForLength(
        radius_.Value(),
        hypotf(box_size.width(), box_size.height()) / sqrtf(2));
  }

  gfx::PointF center = PointForCenterCoordinate(center_x_, center_y_, box_size);

  float width_delta = std::abs(box_size.width() - center.x());
  float height_delta = std::abs(box_size.height() - center.y());
  if (radius_.GetType() == BasicShapeRadius::kClosestSide) {
    return std::min(std::min(std::abs(center.x()), width_delta),
                    std::min(std::abs(center.y()), height_delta));
  }

  // If radius.type() == BasicShapeRadius::kFarthestSide.
  return std::max(std::max(center.x(), width_delta),
                  std::max(center.y(), height_delta));
}

void BasicShapeCircle::GetPath(Path& path,
                               const gfx::RectF& bounding_box,
                               float) {
  DCHECK(path.IsEmpty());
  gfx::PointF center =
      PointForCenterCoordinate(center_x_, center_y_, bounding_box.size());
  float radius = FloatValueForRadiusInBox(bounding_box.size());
  path.AddEllipse(center + bounding_box.OffsetFromOrigin(), radius, radius);
}

bool BasicShapeEllipse::IsEqualAssumingSameType(const BasicShape& o) const {
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

void BasicShapeEllipse::GetPath(Path& path,
                                const gfx::RectF& bounding_box,
                                float) {
  DCHECK(path.IsEmpty());
  gfx::PointF center =
      PointForCenterCoordinate(center_x_, center_y_, bounding_box.size());
  float radius_x =
      FloatValueForRadiusInBox(radius_x_, center.x(), bounding_box.width());
  float radius_y =
      FloatValueForRadiusInBox(radius_y_, center.y(), bounding_box.height());
  path.AddEllipse(center + bounding_box.OffsetFromOrigin(), radius_x, radius_y);
}

void BasicShapePolygon::GetPath(Path& path,
                                const gfx::RectF& bounding_box,
                                float) {
  DCHECK(path.IsEmpty());
  DCHECK(!(values_.size() % 2));
  wtf_size_t length = values_.size();

  if (!length)
    return;

  path.MoveTo(
      gfx::PointF(FloatValueForLength(values_.at(0), bounding_box.width()) +
                      bounding_box.x(),
                  FloatValueForLength(values_.at(1), bounding_box.height()) +
                      bounding_box.y()));
  for (wtf_size_t i = 2; i < length; i = i + 2) {
    path.AddLineTo(gfx::PointF(
        FloatValueForLength(values_.at(i), bounding_box.width()) +
            bounding_box.x(),
        FloatValueForLength(values_.at(i + 1), bounding_box.height()) +
            bounding_box.y()));
  }
  path.CloseSubpath();
}

bool BasicShapePolygon::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapePolygon& other = To<BasicShapePolygon>(o);
  return wind_rule_ == other.wind_rule_ && values_ == other.values_;
}

bool BasicShapeRectCommon::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapeRectCommon& other = To<BasicShapeRectCommon>(o);
  return right_ == other.right_ && top_ == other.top_ &&
         bottom_ == other.bottom_ && left_ == other.left_ &&
         top_left_radius_ == other.top_left_radius_ &&
         top_right_radius_ == other.top_right_radius_ &&
         bottom_right_radius_ == other.bottom_right_radius_ &&
         bottom_left_radius_ == other.bottom_left_radius_;
}

void BasicShapeInset::GetPath(Path& path,
                              const gfx::RectF& bounding_box,
                              float) {
  DCHECK(path.IsEmpty());
  float left = FloatValueForLength(left_, bounding_box.width());
  float top = FloatValueForLength(top_, bounding_box.height());
  gfx::RectF rect(
      left + bounding_box.x(), top + bounding_box.y(),
      std::max<float>(bounding_box.width() - left -
                          FloatValueForLength(right_, bounding_box.width()),
                      0),
      std::max<float>(bounding_box.height() - top -
                          FloatValueForLength(bottom_, bounding_box.height()),
                      0));
  gfx::SizeF box_size = bounding_box.size();
  auto radii = FloatRoundedRect::Radii(
      SizeForLengthSize(top_left_radius_, box_size),
      SizeForLengthSize(top_right_radius_, box_size),
      SizeForLengthSize(bottom_left_radius_, box_size),
      SizeForLengthSize(bottom_right_radius_, box_size));

  FloatRoundedRect final_rect(rect, radii);
  final_rect.ConstrainRadii();
  path.AddRoundedRect(final_rect);
}

void BasicShapeRect::GetPath(Path& path,
                             const gfx::RectF& bounding_box,
                             float) {
  DCHECK(path.IsEmpty());

  // The following computes |left|, |right| as the inset from the left edge
  // of the bounding box and |top|, |bottom| as the inset from the top edge of
  // the bounding box.
  // auto aligns the inset with the corresponding edge. So |left| and |top|
  // align with the left and top edge of the bounding box and compute
  // equivalent to 0%. And |right| and |bottom| align with the right and bottom
  // edge of the bounding box and compute equivalent to 100%.
  float left = left_.GetType() == Length::kAuto
                   ? 0
                   : FloatValueForLength(left_, bounding_box.width());

  float top = top_.GetType() == Length::kAuto
                  ? 0
                  : FloatValueForLength(top_, bounding_box.height());

  float right = right_.GetType() == Length::kAuto
                    ? bounding_box.width()
                    : FloatValueForLength(right_, bounding_box.width());
  right = std::max(right, left);

  float bottom = bottom_.GetType() == Length::kAuto
                     ? bounding_box.height()
                     : FloatValueForLength(bottom_, bounding_box.height());
  bottom = std::max(top, bottom);

  gfx::RectF rect(left + bounding_box.x(), top + bounding_box.y(), right - left,
                  bottom - top);

  gfx::SizeF box_size = bounding_box.size();
  auto radii = FloatRoundedRect::Radii(
      SizeForLengthSize(top_left_radius_, box_size),
      SizeForLengthSize(top_right_radius_, box_size),
      SizeForLengthSize(bottom_left_radius_, box_size),
      SizeForLengthSize(bottom_right_radius_, box_size));

  FloatRoundedRect final_rect(rect, radii);
  final_rect.ConstrainRadii();
  path.AddRoundedRect(final_rect);
}

void BasicShapeXYWH::GetPath(Path& path,
                             const gfx::RectF& bounding_box,
                             float) {
  DCHECK(path.IsEmpty());
  gfx::RectF rect(FloatValueForLength(x_, bounding_box.width()),
                  FloatValueForLength(y_, bounding_box.height()),
                  FloatValueForLength(width_, bounding_box.height()),
                  FloatValueForLength(height_, bounding_box.height()));

  gfx::SizeF box_size = bounding_box.size();
  auto radii = FloatRoundedRect::Radii(
      SizeForLengthSize(top_left_radius_, box_size),
      SizeForLengthSize(top_right_radius_, box_size),
      SizeForLengthSize(bottom_left_radius_, box_size),
      SizeForLengthSize(bottom_right_radius_, box_size));

  FloatRoundedRect final_rect(rect, radii);
  final_rect.ConstrainRadii();
  path.AddRoundedRect(final_rect);
}

bool BasicShapeXYWH::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapeXYWH& other = To<BasicShapeXYWH>(o);
  return x_ == other.x_ && y_ == other.y_ && width_ == other.width_ &&
         height_ == other.height_ &&
         top_left_radius_ == other.top_left_radius_ &&
         top_right_radius_ == other.top_right_radius_ &&
         bottom_right_radius_ == other.bottom_right_radius_ &&
         bottom_left_radius_ == other.bottom_left_radius_;
}

}  // namespace blink
