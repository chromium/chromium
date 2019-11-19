/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_DATA_H_

#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum SVGPathSegType {
  kPathSegUnknown = 0,
  kPathSegClosePath = 1,
  kPathSegMoveToAbs = 2,
  kPathSegMoveToRel = 3,
  kPathSegLineToAbs = 4,
  kPathSegLineToRel = 5,
  kPathSegCurveToCubicAbs = 6,
  kPathSegCurveToCubicRel = 7,
  kPathSegCurveToQuadraticAbs = 8,
  kPathSegCurveToQuadraticRel = 9,
  kPathSegArcAbs = 10,
  kPathSegArcRel = 11,
  kPathSegLineToHorizontalAbs = 12,
  kPathSegLineToHorizontalRel = 13,
  kPathSegLineToVerticalAbs = 14,
  kPathSegLineToVerticalRel = 15,
  kPathSegCurveToCubicSmoothAbs = 16,
  kPathSegCurveToCubicSmoothRel = 17,
  kPathSegCurveToQuadraticSmoothAbs = 18,
  kPathSegCurveToQuadraticSmoothRel = 19
};

static inline SVGPathSegType ToAbsolutePathSegType(const SVGPathSegType type) {
  // Clear the LSB to get the absolute command.
  return type >= kPathSegMoveToAbs ? static_cast<SVGPathSegType>(type & ~1u)
                                   : type;
}

static inline bool IsAbsolutePathSegType(const SVGPathSegType type) {
  // For commands with an ordinal >= PathSegMoveToAbs, and odd number =>
  // relative command.
  return type < kPathSegMoveToAbs || type % 2 == 0;
}

struct PathSegmentData {
  STACK_ALLOCATED();

 public:
  PathSegmentData()
      : command(kPathSegUnknown), arc_sweep(false), arc_large(false) {}

  const FloatPoint& ArcRadii() const { return point1; }
  FloatPoint& ArcRadii() { return point1; }

  float ArcAngle() const { return point2.X(); }
  void SetArcAngle(float angle) { point2.SetX(angle); }

  float R1() const { return ArcRadii().X(); }
  float R2() const { return ArcRadii().Y(); }

  bool LargeArcFlag() const { return arc_large; }
  bool SweepFlag() const { return arc_sweep; }

  float X() const { return target_point.X(); }
  float Y() const { return target_point.Y(); }

  float X1() const { return point1.X(); }
  float Y1() const { return point1.Y(); }

  float X2() const { return point2.X(); }
  float Y2() const { return point2.Y(); }

  SVGPathSegType command;
  FloatPoint target_point;
  FloatPoint point1;
  FloatPoint point2;
  bool arc_sweep;
  bool arc_large;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_DATA_H_
