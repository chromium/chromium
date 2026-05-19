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

#include <algorithm>
#include <cmath>
#include <limits>

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

// Minimum length (in pixels) for edges and rounding radii below which rounding
// is skipped. Values smaller than this won't produce visually noticeable
// rounding (sub-pixel), so we avoid the computation entirely.
constexpr float kMinRoundingThreshold = 0.5f;

struct RoundedPolygonEdge {
  gfx::Vector2dF unit;
  float length = 0.0f;
  bool is_valid_for_rounding = false;

  RoundedPolygonEdge(const gfx::PointF& p1, const gfx::PointF& p2) {
    gfx::Vector2dF vector = p2 - p1;
    length = vector.Length();
    if (length > kMinRoundingThreshold) {
      is_valid_for_rounding = true;
      unit = vector;
      unit.InvScale(length);
    }
  }
};

float GetRoundedPolygonRadius(const RoundedPolygonEdge& incoming,
                              const RoundedPolygonEdge& outgoing,
                              float requested_radius) {
  if (requested_radius <= 0 || !incoming.is_valid_for_rounding ||
      !outgoing.is_valid_for_rounding) {
    return 0.0f;
  }

  // The interior angle at the vertex: the angle between the vectors pointing
  // from the vertex toward the previous and next vertices. Since incoming.unit
  // points INTO the vertex, negate it to get the outward direction.
  const double cos_interior =
      std::clamp(-gfx::DotProduct(incoming.unit, outgoing.unit), -1.0, 1.0);
  const double interior_angle = std::acos(cos_interior);

  if (interior_angle <= std::numeric_limits<double>::epsilon() ||
      std::abs(interior_angle - kPiDouble) <=
          std::numeric_limits<double>::epsilon()) {
    return 0.0f;
  }

  const double tan_half_interior = std::tan(interior_angle / 2.0);
  if (!std::isfinite(tan_half_interior) ||
      tan_half_interior <= std::numeric_limits<double>::epsilon()) {
    return 0.0f;
  }

  // The spec clamps the radius so it never exceeds
  // tan(interior_angle/2) × segment / 2 for either adjacent segment.
  // Spec: https://www.w3.org/TR/css-shapes-1/#funcdef-basic-shape-polygon
  const double max_radius = std::min(tan_half_interior * incoming.length * 0.5,
                                     tan_half_interior * outgoing.length * 0.5);
  const double radius =
      std::min(static_cast<double>(requested_radius), max_radius);

  if (radius <= std::numeric_limits<double>::epsilon()) {
    return 0.0f;
  }

  return ClampTo<float>(radius);
}

// Compute the distance from |center| to the closest or farthest corner of a
// box of size |box_size|. Named similarly to RadiusToCorner in
// css_gradient_value.cc for discoverability.
float RadiusToCorner(const gfx::PointF& center,
                     const gfx::SizeF& box_size,
                     bool closest) {
  float dx0 = center.x();
  float dx1 = box_size.width() - center.x();
  float dy0 = center.y();
  float dy1 = box_size.height() - center.y();

  float dx = closest ? std::min(dx0, dx1) : std::max(dx0, dx1);
  float dy = closest ? std::min(dy0, dy1) : std::max(dy0, dy1);
  return hypotf(dx, dy);
}

}  // namespace

gfx::PointF PointForCenterCoordinate(const BasicShapeCenterCoordinate& center_x,
                                     const BasicShapeCenterCoordinate& center_y,
                                     gfx::SizeF box_size) {
  float x = FloatValueForLength(center_x.ComputedLength(), box_size.width());
  float y = FloatValueForLength(center_y.ComputedLength(), box_size.height());
  return gfx::PointF(x, y);
}

bool BasicShapeCircle::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapeCircle& other = To<BasicShapeCircle>(o);
  return center_x_ == other.center_x_ && center_y_ == other.center_y_ &&
         radius_ == other.radius_;
}

float BasicShapeCircle::FloatValueForRadiusInBox(
    const gfx::PointF& center,
    const gfx::SizeF& box_size) const {
  if (radius_.GetType() == BasicShapeRadius::kValue) {
    return FloatValueForLength(
        radius_.Value(),
        hypotf(box_size.width(), box_size.height()) / sqrtf(2));
  }

  if (radius_.GetType() == BasicShapeRadius::kClosestCorner) {
    return RadiusToCorner(center, box_size, /*closest=*/true);
  }
  if (radius_.GetType() == BasicShapeRadius::kFarthestCorner) {
    return RadiusToCorner(center, box_size, /*closest=*/false);
  }

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

Path BasicShapeCircle::GetPath(const gfx::RectF& bounding_box,
                               float /*zoom*/,
                               float path_scale) const {
  const gfx::PointF center =
      PointForCenterCoordinate(center_x_, center_y_, bounding_box.size());
  return GetPathFromCenter(center, bounding_box, path_scale);
}

Path BasicShapeCircle::GetPathFromCenter(const gfx::PointF& center,
                                         const gfx::RectF& bounding_box,
                                         float path_scale) const {
  const gfx::PointF scaled_center =
      gfx::ScalePoint(center + bounding_box.OffsetFromOrigin(), path_scale);
  const float scaled_radius =
      FloatValueForRadiusInBox(center, bounding_box.size()) * path_scale;

  return Path::MakeEllipse(scaled_center, scaled_radius, scaled_radius);
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
  if (radius.GetType() == BasicShapeRadius::kValue) {
    return FloatValueForLength(radius.Value(), box_width_or_height);
  }

  float width_or_height_delta = std::abs(box_width_or_height - center);
  if (radius.GetType() == BasicShapeRadius::kClosestSide) {
    return std::min(std::abs(center), width_or_height_delta);
  }
  if (radius.GetType() == BasicShapeRadius::kFarthestSide) {
    return std::max(center, width_or_height_delta);
  }

  // closest-corner/farthest-corner require both axes for Euclidean distance.
  // Use ResolveRadii() for those values.
  NOTREACHED();
}

gfx::SizeF BasicShapeEllipse::ResolveRadii(const gfx::PointF& center,
                                           const gfx::SizeF& box_size) const {
  auto resolve_radius = [&](const BasicShapeRadius& radius, float center_coord,
                            float box_dim) -> float {
    if (radius.GetType() == BasicShapeRadius::kClosestCorner) {
      return RadiusToCorner(center, box_size, /*closest=*/true);
    }
    if (radius.GetType() == BasicShapeRadius::kFarthestCorner) {
      return RadiusToCorner(center, box_size, /*closest=*/false);
    }
    return FloatValueForRadiusInBox(radius, center_coord, box_dim);
  };

  return gfx::SizeF(resolve_radius(radius_x_, center.x(), box_size.width()),
                    resolve_radius(radius_y_, center.y(), box_size.height()));
}

Path BasicShapeEllipse::GetPath(const gfx::RectF& bounding_box,
                                float /*zoom*/,
                                float path_scale) const {
  const gfx::PointF center =
      PointForCenterCoordinate(center_x_, center_y_, bounding_box.size());
  return GetPathFromCenter(center, bounding_box, path_scale);
}

Path BasicShapeEllipse::GetPathFromCenter(const gfx::PointF& center,
                                          const gfx::RectF& bounding_box,
                                          float path_scale) const {
  const gfx::PointF scaled_center =
      gfx::ScalePoint(center + bounding_box.OffsetFromOrigin(), path_scale);
  const gfx::SizeF radii = ResolveRadii(center, bounding_box.size());
  const gfx::SizeF scaled_radii = gfx::ScaleSize(radii, path_scale);

  return Path::MakeEllipse(scaled_center, scaled_radii.width(),
                           scaled_radii.height());
}

Path BasicShapePolygon::GetPath(const gfx::RectF& bounding_box,
                                float /*zoom*/,
                                float path_scale) const {
  DCHECK(!(values_.size() % 2));
  wtf_size_t length = values_.size();

  PathBuilder builder;
  builder.SetWindRule(wind_rule_);
  if (!length) {
    return builder.Finalize();
  }

  Vector<gfx::PointF> points(length / 2);
  for (wtf_size_t i = 0; i < length; i += 2) {
    points[i / 2] = gfx::ScalePoint(
        gfx::PointF(
            FloatValueForLength(values_.at(i), bounding_box.width()) +
                bounding_box.x(),
            FloatValueForLength(values_.at(i + 1), bounding_box.height()) +
                bounding_box.y()),
        path_scale);
  }

  // The rounding radius is a <length>, so the reference size for percentage
  // resolution is irrelevant here.
  const float requested_radius =
      FloatValueForLength(rounding_radius_, 0) * path_scale;
  if (requested_radius <= kMinRoundingThreshold || points.size() < 3) {
    builder.MoveTo(points.front());
    for (wtf_size_t i = 1; i < points.size(); ++i) {
      builder.LineTo(points.at(i));
    }
    builder.Close();
    return builder.Finalize();
  }

  // Start the path at the midpoint between the last and first vertices,
  // rather than exactly at points.front(). This achieves two things:
  // 1. Tangent Generation: The underlying Skia implementation of arcTo(SkPoint
  //    p1, SkPoint p2, SkScalar radius) requires an existing path coordinate
  //    to construct the incoming tangent ray.
  // 2. Correctness: To prevent the final close() command from drawing backwards
  //    across the last rounded corner, AND to prevent the first arcTo() from
  //    drawing backwards because the initial position overshot the arc's start,
  //    the path's starting position must be outside the radii of both arcs.
  //    It must also be collinear with the original edge. Because the CSS spec
  //    clamps rounding radii to never consume more than 50% of an adjacent
  //    segment, the segment's midpoint is the only point guaranteed to satisfy
  //    all these conditions.
  //
  //    https://www.w3.org/TR/css-shapes-1/#funcdef-basic-shape-polygon
  const gfx::PointF& p_last = points.back();
  const gfx::PointF& p_first = points.front();
  builder.MoveTo(gfx::PointF(0.5f * p_last.x() + 0.5f * p_first.x(),
                             0.5f * p_last.y() + 0.5f * p_first.y()));
  RoundedPolygonEdge incoming_edge(p_last, p_first);
  for (wtf_size_t i = 0; i < points.size(); ++i) {
    const gfx::PointF& current = points[i];
    const gfx::PointF& next = points[(i + 1) % points.size()];

    RoundedPolygonEdge outgoing_edge(current, next);

    float radius =
        GetRoundedPolygonRadius(incoming_edge, outgoing_edge, requested_radius);

    if (radius > 0.0f) {
      builder.ArcTo(current, next, radius);
    } else {
      builder.LineTo(current);
    }
    incoming_edge = outgoing_edge;
  }

  builder.Close();
  return builder.Finalize();
}

bool BasicShapePolygon::IsEqualAssumingSameType(const BasicShape& o) const {
  const BasicShapePolygon& other = To<BasicShapePolygon>(o);
  return wind_rule_ == other.wind_rule_ &&
         rounding_radius_ == other.rounding_radius_ && values_ == other.values_;
}

bool BasicShapeInset::IsEqualAssumingSameType(const BasicShape& o) const {
  const auto& other = To<BasicShapeInset>(o);
  return right_ == other.right_ && top_ == other.top_ &&
         bottom_ == other.bottom_ && left_ == other.left_ &&
         top_left_radius_ == other.top_left_radius_ &&
         top_right_radius_ == other.top_right_radius_ &&
         bottom_right_radius_ == other.bottom_right_radius_ &&
         bottom_left_radius_ == other.bottom_left_radius_;
}

Path BasicShapeInset::GetPath(const gfx::RectF& bounding_box,
                              float /*zoom*/,
                              float path_scale) const {
  const float left = FloatValueForLength(left_, bounding_box.width());
  const float top = FloatValueForLength(top_, bounding_box.height());
  const gfx::RectF scaled_rect = gfx::ScaleRect(
      gfx::RectF(
          left + bounding_box.x(), top + bounding_box.y(),
          std::max<float>(bounding_box.width() - left -
                              FloatValueForLength(right_, bounding_box.width()),
                          0),
          std::max<float>(
              bounding_box.height() - top -
                  FloatValueForLength(bottom_, bounding_box.height()),
              0)),
      path_scale);
  const gfx::SizeF box_size = bounding_box.size();
  const auto scaled_radii = FloatRoundedRect::Radii(
      gfx::ScaleSize(SizeForLengthSize(top_left_radius_, box_size), path_scale),
      gfx::ScaleSize(SizeForLengthSize(top_right_radius_, box_size),
                     path_scale),
      gfx::ScaleSize(SizeForLengthSize(bottom_left_radius_, box_size),
                     path_scale),
      gfx::ScaleSize(SizeForLengthSize(bottom_right_radius_, box_size),
                     path_scale));

  FloatRoundedRect final_rect(scaled_rect, scaled_radii);
  final_rect.ConstrainRadii();

  return Path::MakeRoundedRect(final_rect);
}

}  // namespace blink
