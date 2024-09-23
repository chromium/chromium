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

#include "third_party/blink/renderer/core/svg/svg_path_builder.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

gfx::PointF SVGPathBuilder::SmoothControl(bool is_compatible_segment) const {
  // The control point is assumed to be the reflection of the control point on
  // the previous command relative to the current point. If there is no previous
  // command or if the previous command was not a [quad/cubic], assume the
  // control point is coincident with the current point.
  // [https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands]
  // [https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands]
  gfx::PointF control_point = current_point_;
  if (is_compatible_segment)
    control_point += current_point_ - last_control_point_;

  return control_point;
}

void SVGPathBuilder::EmitClose() {
  path_.CloseSubpath();

  // At the end of the [closepath] command, the new current
  // point is set to the initial point of the current subpath.
  // [https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand]
  current_point_ = subpath_point_;
}

void SVGPathBuilder::EmitMoveTo(const gfx::PointF& p) {
  path_.MoveTo(p);

  subpath_point_ = p;
  current_point_ = p;
}

void SVGPathBuilder::EmitLineTo(const gfx::PointF& p) {
  path_.AddLineTo(p);
  current_point_ = p;
}

void SVGPathBuilder::EmitQuadTo(const gfx::PointF& c0, const gfx::PointF& p) {
  path_.AddQuadCurveTo(c0, p);
  last_control_point_ = c0;
  current_point_ = p;
}

void SVGPathBuilder::EmitSmoothQuadTo(const gfx::PointF& p) {
  bool last_was_quadratic =
      last_command_ == kPathSegCurveToQuadraticAbs ||
      last_command_ == kPathSegCurveToQuadraticRel ||
      last_command_ == kPathSegCurveToQuadraticSmoothAbs ||
      last_command_ == kPathSegCurveToQuadraticSmoothRel;

  EmitQuadTo(SmoothControl(last_was_quadratic), p);
}

void SVGPathBuilder::EmitCubicTo(const gfx::PointF& c0,
                                 const gfx::PointF& c1,
                                 const gfx::PointF& p) {
  path_.AddBezierCurveTo(c0, c1, p);
  last_control_point_ = c1;
  current_point_ = p;
}

void SVGPathBuilder::EmitSmoothCubicTo(const gfx::PointF& c1,
                                       const gfx::PointF& p) {
  bool last_was_cubic = last_command_ == kPathSegCurveToCubicAbs ||
                        last_command_ == kPathSegCurveToCubicRel ||
                        last_command_ == kPathSegCurveToCubicSmoothAbs ||
                        last_command_ == kPathSegCurveToCubicSmoothRel;

  EmitCubicTo(SmoothControl(last_was_cubic), c1, p);
}

void SVGPathBuilder::EmitArcTo(const gfx::PointF& p,
                               float radius_x,
                               float radius_y,
                               float rotate,
                               bool large_arc,
                               bool sweep) {
  path_.AddArcTo(p, radius_x, radius_y, rotate, large_arc, sweep);
  current_point_ = p;
}

void SVGPathBuilder::EmitSegment(const PathSegmentData& segment) {
  switch (segment.command) {
    case kPathSegClosePath:
      EmitClose();
      break;
    case kPathSegMoveToAbs:
      EmitMoveTo(segment.target_point);
      break;
    case kPathSegMoveToRel:
      EmitMoveTo(current_point_ + segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegLineToAbs:
      EmitLineTo(segment.target_point);
      break;
    case kPathSegLineToRel:
      EmitLineTo(current_point_ + segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegLineToHorizontalAbs:
      EmitLineTo(gfx::PointF(segment.target_point.x(), current_point_.y()));
      break;
    case kPathSegLineToHorizontalRel:
      EmitLineTo(current_point_ + gfx::Vector2dF(segment.target_point.x(), 0));
      break;
    case kPathSegLineToVerticalAbs:
      EmitLineTo(gfx::PointF(current_point_.x(), segment.target_point.y()));
      break;
    case kPathSegLineToVerticalRel:
      EmitLineTo(current_point_ + gfx::Vector2dF(0, segment.target_point.y()));
      break;
    case kPathSegCurveToQuadraticAbs:
      EmitQuadTo(segment.point1, segment.target_point);
      break;
    case kPathSegCurveToQuadraticRel:
      EmitQuadTo(current_point_ + segment.point1.OffsetFromOrigin(),
                 current_point_ + segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegCurveToQuadraticSmoothAbs:
      EmitSmoothQuadTo(segment.target_point);
      break;
    case kPathSegCurveToQuadraticSmoothRel:
      EmitSmoothQuadTo(current_point_ +
                       segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegCurveToCubicAbs:
      EmitCubicTo(segment.point1, segment.point2, segment.target_point);
      break;
    case kPathSegCurveToCubicRel:
      EmitCubicTo(current_point_ + segment.point1.OffsetFromOrigin(),
                  current_point_ + segment.point2.OffsetFromOrigin(),
                  current_point_ + segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegCurveToCubicSmoothAbs:
      EmitSmoothCubicTo(segment.point2, segment.target_point);
      break;
    case kPathSegCurveToCubicSmoothRel:
      EmitSmoothCubicTo(
          current_point_ + segment.point2.OffsetFromOrigin(),
          current_point_ + segment.target_point.OffsetFromOrigin());
      break;
    case kPathSegArcAbs:
      EmitArcTo(segment.target_point, segment.ArcRadiusX(),
                segment.ArcRadiusY(), segment.ArcAngle(),
                segment.LargeArcFlag(), segment.SweepFlag());
      break;
    case kPathSegArcRel:
      EmitArcTo(current_point_ + segment.target_point.OffsetFromOrigin(),
                segment.ArcRadiusX(), segment.ArcRadiusY(), segment.ArcAngle(),
                segment.LargeArcFlag(), segment.SweepFlag());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  last_command_ = segment.command;
}

}  // namespace blink
