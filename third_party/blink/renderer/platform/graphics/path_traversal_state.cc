/*
 * Copyright (C) 2006, 2007 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/platform/graphics/path_traversal_state.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static inline gfx::PointF MidPoint(const gfx::PointF& first,
                                   const gfx::PointF& second) {
  return gfx::PointF((first.x() + second.x()) / 2.0f,
                     (first.y() + second.y()) / 2.0f);
}

static inline float DistanceLine(const gfx::PointF& start,
                                 const gfx::PointF& end) {
  return (end - start).Length();
}

static inline double DotSelf(const gfx::PointF& p) {
  return p.OffsetFromOrigin().LengthSquared();
}

struct QuadraticBezier {
  DISALLOW_NEW();
  QuadraticBezier() = default;
  QuadraticBezier(const gfx::PointF& s,
                  const gfx::PointF& c,
                  const gfx::PointF& e)
      : start(s), control(c), end(e), split_depth(0) {}

  double MagnitudeSquared() const {
    return (DotSelf(start) + DotSelf(control) + DotSelf(end)) / 9.0;
  }

  float ApproximateDistance() const {
    return DistanceLine(start, control) + DistanceLine(control, end);
  }

  void Split(QuadraticBezier& left, QuadraticBezier& right) const {
    left.control = MidPoint(start, control);
    right.control = MidPoint(control, end);

    gfx::PointF left_control_to_right_control =
        MidPoint(left.control, right.control);
    left.end = left_control_to_right_control;
    right.start = left_control_to_right_control;

    left.start = start;
    right.end = end;

    left.split_depth = right.split_depth = split_depth + 1;
  }

  gfx::PointF start;
  gfx::PointF control;
  gfx::PointF end;
  uint16_t split_depth;
};

struct CubicBezier {
  DISALLOW_NEW();
  CubicBezier() = default;
  CubicBezier(const gfx::PointF& s,
              const gfx::PointF& c1,
              const gfx::PointF& c2,
              const gfx::PointF& e)
      : start(s), control1(c1), control2(c2), end(e), split_depth(0) {}

  double MagnitudeSquared() const {
    return (DotSelf(start) + DotSelf(control1) + DotSelf(control2) +
            DotSelf(end)) /
           16.0;
  }

  float ApproximateDistance() const {
    return DistanceLine(start, control1) + DistanceLine(control1, control2) +
           DistanceLine(control2, end);
  }

  void Split(CubicBezier& left, CubicBezier& right) const {
    gfx::PointF start_to_control1 = MidPoint(control1, control2);

    left.start = start;
    left.control1 = MidPoint(start, control1);
    left.control2 = MidPoint(left.control1, start_to_control1);

    right.control2 = MidPoint(control2, end);
    right.control1 = MidPoint(right.control2, start_to_control1);
    right.end = end;

    gfx::PointF left_control2_to_right_control1 =
        MidPoint(left.control2, right.control1);
    left.end = left_control2_to_right_control1;
    right.start = left_control2_to_right_control1;

    left.split_depth = right.split_depth = split_depth + 1;
  }

  gfx::PointF start;
  gfx::PointF control1;
  gfx::PointF control2;
  gfx::PointF end;
  uint16_t split_depth;
};

template <class CurveType>
static float CurveLength(PathTraversalState& traversal_state, CurveType curve) {
  static const uint16_t kCurveSplitDepthLimit = 20;
  static const double kPathSegmentLengthToleranceSquared = 1.e-16;

  double curve_scale_for_tolerance_squared = curve.MagnitudeSquared();
  if (curve_scale_for_tolerance_squared < kPathSegmentLengthToleranceSquared)
    return 0;

  Vector<CurveType> curve_stack;
  curve_stack.push_back(curve);

  float total_length = 0;
  do {
    float length = curve.ApproximateDistance();
    double length_discrepancy = length - DistanceLine(curve.start, curve.end);
    if ((length_discrepancy * length_discrepancy) /
                curve_scale_for_tolerance_squared >
            kPathSegmentLengthToleranceSquared &&
        curve.split_depth < kCurveSplitDepthLimit) {
      CurveType left_curve;
      CurveType right_curve;
      curve.Split(left_curve, right_curve);
      curve = left_curve;
      curve_stack.push_back(right_curve);
    } else {
      total_length += length;
      if (traversal_state.action_ ==
              PathTraversalState::kTraversalPointAtLength ||
          traversal_state.action_ ==
              PathTraversalState::kTraversalNormalAngleAtLength) {
        traversal_state.previous_ = curve.start;
        traversal_state.current_ = curve.end;
        if (traversal_state.total_length_ + total_length >
            traversal_state.desired_length_)
          return total_length;
      }
      curve = curve_stack.back();
      curve_stack.pop_back();
    }
  } while (!curve_stack.empty());

  return total_length;
}

PathTraversalState::PathTraversalState(PathTraversalAction action)
    : action_(action),
      success_(false),
      total_length_(0),
      desired_length_(0),
      normal_angle_(0) {}

float PathTraversalState::CloseSubpath() {
  float distance = DistanceLine(current_, start_);
  current_ = start_;
  return distance;
}

float PathTraversalState::MoveTo(const gfx::PointF& point) {
  current_ = start_ = point;
  return 0;
}

float PathTraversalState::LineTo(const gfx::PointF& point) {
  float distance = DistanceLine(current_, point);
  current_ = point;
  return distance;
}

float PathTraversalState::CubicBezierTo(const gfx::PointF& new_control1,
                                        const gfx::PointF& new_control2,
                                        const gfx::PointF& new_end) {
  float distance = CurveLength<CubicBezier>(
      *this, CubicBezier(current_, new_control1, new_control2, new_end));

  if (action_ != kTraversalPointAtLength &&
      action_ != kTraversalNormalAngleAtLength)
    current_ = new_end;

  return distance;
}

void PathTraversalState::ProcessSegment() {
  if ((action_ == kTraversalPointAtLength ||
       action_ == kTraversalNormalAngleAtLength) &&
      total_length_ >= desired_length_) {
    float slope = (current_ - previous_).SlopeAngleRadians();
    if (action_ == kTraversalPointAtLength) {
      float offset = desired_length_ - total_length_;
      current_.Offset(offset * cosf(slope), offset * sinf(slope));
    } else {
      normal_angle_ = Rad2deg(slope);
    }
    success_ = true;
  }
  previous_ = current_;
}

}  // namespace blink
