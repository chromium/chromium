/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_path_parser.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_path_consumer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static gfx::PointF ReflectedPoint(const gfx::PointF& reflect_in,
                                  const gfx::PointF& point_to_reflect) {
  return gfx::PointF(2 * reflect_in.x() - point_to_reflect.x(),
                     2 * reflect_in.y() - point_to_reflect.y());
}

// Blend the points with a ratio (1/3):(2/3).
static gfx::PointF BlendPoints(const gfx::PointF& p1, const gfx::PointF& p2) {
  const float kOneOverThree = 1 / 3.f;
  return gfx::PointF((p1.x() + 2 * p2.x()) * kOneOverThree,
                     (p1.y() + 2 * p2.y()) * kOneOverThree);
}

static inline bool IsCubicCommand(SVGPathSegType command) {
  return command == kPathSegCurveToCubicAbs ||
         command == kPathSegCurveToCubicRel ||
         command == kPathSegCurveToCubicSmoothAbs ||
         command == kPathSegCurveToCubicSmoothRel;
}

static inline bool IsQuadraticCommand(SVGPathSegType command) {
  return command == kPathSegCurveToQuadraticAbs ||
         command == kPathSegCurveToQuadraticRel ||
         command == kPathSegCurveToQuadraticSmoothAbs ||
         command == kPathSegCurveToQuadraticSmoothRel;
}

void SVGPathNormalizer::EmitSegment(const PathSegmentData& segment) {
  PathSegmentData norm_seg = segment;

  // Convert relative points to absolute points.
  switch (segment.command) {
    case kPathSegCurveToQuadraticRel:
      norm_seg.point1 += current_point_.OffsetFromOrigin();
      norm_seg.target_point += current_point_.OffsetFromOrigin();
      break;
    case kPathSegCurveToCubicRel:
      norm_seg.point1 += current_point_.OffsetFromOrigin();
      [[fallthrough]];
    case kPathSegCurveToCubicSmoothRel:
      norm_seg.point2 += current_point_.OffsetFromOrigin();
      [[fallthrough]];
    case kPathSegMoveToRel:
    case kPathSegLineToRel:
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToVerticalRel:
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegArcRel:
      norm_seg.target_point += current_point_.OffsetFromOrigin();
      break;
    case kPathSegLineToHorizontalAbs:
      norm_seg.target_point.set_y(current_point_.y());
      break;
    case kPathSegLineToVerticalAbs:
      norm_seg.target_point.set_x(current_point_.x());
      break;
    case kPathSegClosePath:
      // Reset m_currentPoint for the next path.
      norm_seg.target_point = sub_path_point_;
      break;
    default:
      break;
  }

  // Update command verb, handle smooth segments and convert quadratic curve
  // segments to cubics.
  switch (segment.command) {
    case kPathSegMoveToRel:
    case kPathSegMoveToAbs:
      sub_path_point_ = norm_seg.target_point;
      norm_seg.command = kPathSegMoveToAbs;
      break;
    case kPathSegLineToRel:
    case kPathSegLineToAbs:
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToHorizontalAbs:
    case kPathSegLineToVerticalRel:
    case kPathSegLineToVerticalAbs:
      norm_seg.command = kPathSegLineToAbs;
      break;
    case kPathSegClosePath:
      norm_seg.command = kPathSegClosePath;
      break;
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToCubicSmoothAbs:
      if (!IsCubicCommand(last_command_))
        norm_seg.point1 = current_point_;
      else
        norm_seg.point1 = ReflectedPoint(current_point_, control_point_);
      [[fallthrough]];
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToCubicAbs:
      control_point_ = norm_seg.point2;
      norm_seg.command = kPathSegCurveToCubicAbs;
      break;
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegCurveToQuadraticSmoothAbs:
      if (!IsQuadraticCommand(last_command_))
        norm_seg.point1 = current_point_;
      else
        norm_seg.point1 = ReflectedPoint(current_point_, control_point_);
      [[fallthrough]];
    case kPathSegCurveToQuadraticRel:
    case kPathSegCurveToQuadraticAbs:
      // Save the unmodified control point.
      control_point_ = norm_seg.point1;
      norm_seg.point1 = BlendPoints(current_point_, control_point_);
      norm_seg.point2 = BlendPoints(norm_seg.target_point, control_point_);
      norm_seg.command = kPathSegCurveToCubicAbs;
      break;
    case kPathSegArcRel:
    case kPathSegArcAbs:
      if (!DecomposeArcToCubic(current_point_, norm_seg)) {
        // On failure, emit a line segment to the target point.
        norm_seg.command = kPathSegLineToAbs;
      } else {
        // decomposeArcToCubic() has already emitted the normalized
        // segments, so set command to PathSegArcAbs, to skip any further
        // emit.
        norm_seg.command = kPathSegArcAbs;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (norm_seg.command != kPathSegArcAbs)
    consumer_->EmitSegment(norm_seg);

  current_point_ = norm_seg.target_point;

  if (!IsCubicCommand(segment.command) && !IsQuadraticCommand(segment.command))
    control_point_ = current_point_;

  last_command_ = segment.command;
}

// This works by converting the SVG arc to "simple" beziers.
// Partly adapted from Niko's code in kdelibs/kdecore/svgicons.
// See also SVG implementation notes:
// http://www.w3.org/TR/SVG/implnote.html#ArcConversionEndpointToCenter
bool SVGPathNormalizer::DecomposeArcToCubic(
    const gfx::PointF& current_point,
    const PathSegmentData& arc_segment) {
  // If rx = 0 or ry = 0 then this arc is treated as a straight line segment (a
  // "lineto") joining the endpoints.
  // http://www.w3.org/TR/SVG/implnote.html#ArcOutOfRangeParameters
  float rx = fabsf(arc_segment.ArcRadiusX());
  float ry = fabsf(arc_segment.ArcRadiusY());
  if (!rx || !ry)
    return false;

  // If the current point and target point for the arc are identical, it should
  // be treated as a zero length path. This ensures continuity in animations.
  if (arc_segment.target_point == current_point)
    return false;

  float angle = arc_segment.ArcAngle();

  gfx::Vector2dF mid_point_distance = current_point - arc_segment.target_point;
  mid_point_distance.Scale(0.5f);

  AffineTransform point_transform;
  point_transform.Rotate(-angle);

  gfx::PointF transformed_mid_point = point_transform.MapPoint(
      gfx::PointF(mid_point_distance.x(), mid_point_distance.y()));
  float square_rx = rx * rx;
  float square_ry = ry * ry;
  float square_x = transformed_mid_point.x() * transformed_mid_point.x();
  float square_y = transformed_mid_point.y() * transformed_mid_point.y();

  // Check if the radii are big enough to draw the arc, scale radii if not.
  // http://www.w3.org/TR/SVG/implnote.html#ArcCorrectionOutOfRangeRadii
  float radii_scale = square_x / square_rx + square_y / square_ry;
  if (radii_scale > 1) {
    rx *= sqrtf(radii_scale);
    ry *= sqrtf(radii_scale);
  }

  point_transform.MakeIdentity();
  point_transform.Scale(1 / rx, 1 / ry);
  point_transform.Rotate(-angle);

  gfx::PointF point1 = point_transform.MapPoint(current_point);
  gfx::PointF point2 = point_transform.MapPoint(arc_segment.target_point);
  gfx::Vector2dF delta = point2 - point1;

  double scale_factor_squared = std::max(1 / delta.LengthSquared() - 0.25, 0.);
  float scale_factor = ClampTo<float>(sqrt(scale_factor_squared));
  if (arc_segment.arc_sweep == arc_segment.arc_large)
    scale_factor = -scale_factor;

  delta.Scale(scale_factor);
  gfx::PointF center_point = point1 + point2.OffsetFromOrigin();
  center_point.Scale(0.5f, 0.5f);
  center_point.Offset(-delta.y(), delta.x());

  float theta1 = (point1 - center_point).SlopeAngleRadians();
  float theta2 = (point2 - center_point).SlopeAngleRadians();

  float theta_arc = theta2 - theta1;
  if (theta_arc < 0 && arc_segment.arc_sweep)
    theta_arc += kTwoPiFloat;
  else if (theta_arc > 0 && !arc_segment.arc_sweep)
    theta_arc -= kTwoPiFloat;

  point_transform.MakeIdentity();
  point_transform.Rotate(angle);
  point_transform.Scale(rx, ry);

  // Some results of atan2 on some platform implementations are not exact
  // enough. So that we get more cubic curves than expected here. Adding 0.001f
  // reduces the count of sgements to the correct count.
  int segments = ceilf(fabsf(theta_arc / (kPiOverTwoFloat + 0.001f)));
  for (int i = 0; i < segments; ++i) {
    float start_theta = theta1 + i * theta_arc / segments;
    float end_theta = theta1 + (i + 1) * theta_arc / segments;

    float t = (8 / 6.f) * tanf(0.25f * (end_theta - start_theta));
    if (!std::isfinite(t))
      return false;
    float sin_start_theta = sinf(start_theta);
    float cos_start_theta = cosf(start_theta);
    float sin_end_theta = sinf(end_theta);
    float cos_end_theta = cosf(end_theta);

    point1 = gfx::PointF(cos_start_theta - t * sin_start_theta,
                         sin_start_theta + t * cos_start_theta);
    point1.Offset(center_point.x(), center_point.y());
    gfx::PointF target_point(cos_end_theta, sin_end_theta);
    target_point.Offset(center_point.x(), center_point.y());
    point2 = target_point;
    point2.Offset(t * sin_end_theta, -t * cos_end_theta);

    PathSegmentData cubic_segment;
    cubic_segment.command = kPathSegCurveToCubicAbs;
    cubic_segment.point1 = point_transform.MapPoint(point1);
    cubic_segment.point2 = point_transform.MapPoint(point2);
    cubic_segment.target_point = point_transform.MapPoint(target_point);

    consumer_->EmitSegment(cubic_segment);
  }
  return true;
}

void SVGPathAbsolutizer::EmitSegment(const PathSegmentData& segment) {
  PathSegmentData absolute_segment = segment;
  if (!IsAbsolutePathSegType(segment.command)) {
    absolute_segment.command = ToAbsolutePathSegType(segment.command);
    gfx::Vector2dF current_point_as_offset = current_point_.OffsetFromOrigin();
    if (segment.command != kPathSegArcRel) {
      absolute_segment.point1 += current_point_as_offset;
      absolute_segment.point2 += current_point_as_offset;
    }
    absolute_segment.target_point += current_point_as_offset;
  }
  consumer_->EmitSegment(absolute_segment);

  if (absolute_segment.command == kPathSegClosePath) {
    current_point_ = sub_path_point_;
  } else if (absolute_segment.command == kPathSegLineToHorizontalAbs) {
    current_point_.set_x(absolute_segment.target_point.x());
  } else if (absolute_segment.command == kPathSegLineToVerticalAbs) {
    current_point_.set_y(absolute_segment.target_point.y());
  } else {
    current_point_ = absolute_segment.target_point;
    if (absolute_segment.command == kPathSegMoveToAbs) {
      sub_path_point_ = current_point_;
    }
  }
}

}  // namespace blink
