/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_path_string_builder.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

String SVGPathStringBuilder::Result() {
  unsigned size = string_builder_.length();
  if (!size)
    return String();

  // Remove trailing space.
  string_builder_.Resize(size - 1);
  return string_builder_.ToString();
}

static void AppendFloat(StringBuilder& string_builder, float value) {
  string_builder.Append(' ');
  string_builder.AppendNumber(value);
}

static void AppendBool(StringBuilder& string_builder, bool value) {
  string_builder.Append(' ');
  string_builder.AppendNumber(value);
}

static void AppendPoint(StringBuilder& string_builder,
                        const gfx::PointF& point) {
  AppendFloat(string_builder, point.x());
  AppendFloat(string_builder, point.y());
}

// TODO(fs): Centralized location for this (SVGPathSeg.h?)
static const auto kPathSegmentCharacter = std::to_array<char>({
    0,    // PathSegUnknown
    'Z',  // PathSegClosePath
    'M',  // PathSegMoveToAbs
    'm',  // PathSegMoveToRel
    'L',  // PathSegLineToAbs
    'l',  // PathSegLineToRel
    'C',  // PathSegCurveToCubicAbs
    'c',  // PathSegCurveToCubicRel
    'Q',  // PathSegCurveToQuadraticAbs
    'q',  // PathSegCurveToQuadraticRel
    'A',  // PathSegArcAbs
    'a',  // PathSegArcRel
    'H',  // PathSegLineToHorizontalAbs
    'h',  // PathSegLineToHorizontalRel
    'V',  // PathSegLineToVerticalAbs
    'v',  // PathSegLineToVerticalRel
    'S',  // PathSegCurveToCubicSmoothAbs
    's',  // PathSegCurveToCubicSmoothRel
    'T',  // PathSegCurveToQuadraticSmoothAbs
    't',  // PathSegCurveToQuadraticSmoothRel
});

void SVGPathStringBuilder::EmitSegment(const PathSegmentData& segment) {
  DCHECK_GT(segment.command, kPathSegUnknown);
  DCHECK_LE(segment.command, kPathSegCurveToQuadraticSmoothRel);
  string_builder_.Append(kPathSegmentCharacter[segment.command]);

  switch (segment.command) {
    case kPathSegMoveToRel:
    case kPathSegMoveToAbs:
    case kPathSegLineToRel:
    case kPathSegLineToAbs:
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegCurveToQuadraticSmoothAbs:
      AppendPoint(string_builder_, segment.target_point);
      break;
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToHorizontalAbs:
      AppendFloat(string_builder_, segment.target_point.x());
      break;
    case kPathSegLineToVerticalRel:
    case kPathSegLineToVerticalAbs:
      AppendFloat(string_builder_, segment.target_point.y());
      break;
    case kPathSegClosePath:
      break;
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToCubicAbs:
      AppendPoint(string_builder_, segment.point1);
      AppendPoint(string_builder_, segment.point2);
      AppendPoint(string_builder_, segment.target_point);
      break;
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToCubicSmoothAbs:
      AppendPoint(string_builder_, segment.point2);
      AppendPoint(string_builder_, segment.target_point);
      break;
    case kPathSegCurveToQuadraticRel:
    case kPathSegCurveToQuadraticAbs:
      AppendPoint(string_builder_, segment.point1);
      AppendPoint(string_builder_, segment.target_point);
      break;
    case kPathSegArcRel:
    case kPathSegArcAbs:
      AppendPoint(string_builder_, segment.point1);
      AppendFloat(string_builder_, segment.point2.x());
      AppendBool(string_builder_, segment.arc_large);
      AppendBool(string_builder_, segment.arc_sweep);
      AppendPoint(string_builder_, segment.target_point);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  string_builder_.Append(' ');
}

}  // namespace blink
