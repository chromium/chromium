// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_path_seg_interpolation_functions.h"

#include <memory>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

std::unique_ptr<InterpolableNumber> ConsumeControlAxis(double value,
                                                       bool is_absolute,
                                                       double current_value) {
  return std::make_unique<InterpolableNumber>(
      is_absolute ? value : current_value + value);
}

float ConsumeInterpolableControlAxis(const InterpolableValue* number,
                                     bool is_absolute,
                                     double current_value) {
  double value = ToInterpolableNumber(number)->Value();
  return clampTo<float>(is_absolute ? value : value - current_value);
}

std::unique_ptr<InterpolableNumber>
ConsumeCoordinateAxis(double value, bool is_absolute, double& current_value) {
  if (is_absolute)
    current_value = value;
  else
    current_value += value;
  return std::make_unique<InterpolableNumber>(current_value);
}

float ConsumeInterpolableCoordinateAxis(const InterpolableValue* number,
                                        bool is_absolute,
                                        double& current_value) {
  double previous_value = current_value;
  current_value = ToInterpolableNumber(number)->Value();
  return clampTo<float>(is_absolute ? current_value
                                    : current_value - previous_value);
}

std::unique_ptr<InterpolableValue> ConsumeClosePath(
    const PathSegmentData&,
    PathCoordinates& coordinates) {
  coordinates.current_x = coordinates.initial_x;
  coordinates.current_y = coordinates.initial_y;
  return std::make_unique<InterpolableList>(0);
}

PathSegmentData ConsumeInterpolableClosePath(const InterpolableValue&,
                                             SVGPathSegType seg_type,
                                             PathCoordinates& coordinates) {
  coordinates.current_x = coordinates.initial_x;
  coordinates.current_y = coordinates.initial_y;

  PathSegmentData segment;
  segment.command = seg_type;
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeSingleCoordinate(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto result = std::make_unique<InterpolableList>(2);
  result->Set(0, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(1, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));

  if (ToAbsolutePathSegType(segment.command) == kPathSegMoveToAbs) {
    // Any upcoming 'closepath' commands bring us back to the location we have
    // just moved to.
    coordinates.initial_x = coordinates.current_x;
    coordinates.initial_y = coordinates.current_y;
  }

  return std::move(result);
}

PathSegmentData ConsumeInterpolableSingleCoordinate(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  const InterpolableList& list = ToInterpolableList(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      list.Get(0), is_absolute, coordinates.current_x));
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      list.Get(1), is_absolute, coordinates.current_y));

  if (ToAbsolutePathSegType(seg_type) == kPathSegMoveToAbs) {
    // Any upcoming 'closepath' commands bring us back to the location we have
    // just moved to.
    coordinates.initial_x = coordinates.current_x;
    coordinates.initial_y = coordinates.current_y;
  }

  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeCurvetoCubic(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto result = std::make_unique<InterpolableList>(6);
  result->Set(
      0, ConsumeControlAxis(segment.X1(), is_absolute, coordinates.current_x));
  result->Set(
      1, ConsumeControlAxis(segment.Y1(), is_absolute, coordinates.current_y));
  result->Set(
      2, ConsumeControlAxis(segment.X2(), is_absolute, coordinates.current_x));
  result->Set(
      3, ConsumeControlAxis(segment.Y2(), is_absolute, coordinates.current_y));
  result->Set(4, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(5, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  return std::move(result);
}

PathSegmentData ConsumeInterpolableCurvetoCubic(const InterpolableValue& value,
                                                SVGPathSegType seg_type,
                                                PathCoordinates& coordinates) {
  const InterpolableList& list = ToInterpolableList(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point1.SetX(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                     coordinates.current_x));
  segment.point1.SetY(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                     coordinates.current_y));
  segment.point2.SetX(ConsumeInterpolableControlAxis(list.Get(2), is_absolute,
                                                     coordinates.current_x));
  segment.point2.SetY(ConsumeInterpolableControlAxis(list.Get(3), is_absolute,
                                                     coordinates.current_y));
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      list.Get(4), is_absolute, coordinates.current_x));
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      list.Get(5), is_absolute, coordinates.current_y));
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeCurvetoQuadratic(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto result = std::make_unique<InterpolableList>(4);
  result->Set(
      0, ConsumeControlAxis(segment.X1(), is_absolute, coordinates.current_x));
  result->Set(
      1, ConsumeControlAxis(segment.Y1(), is_absolute, coordinates.current_y));
  result->Set(2, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(3, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  return std::move(result);
}

PathSegmentData ConsumeInterpolableCurvetoQuadratic(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  const InterpolableList& list = ToInterpolableList(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point1.SetX(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                     coordinates.current_x));
  segment.point1.SetY(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                     coordinates.current_y));
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      list.Get(2), is_absolute, coordinates.current_x));
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      list.Get(3), is_absolute, coordinates.current_y));
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeArc(const PathSegmentData& segment,
                                              PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto result = std::make_unique<InterpolableList>(7);
  result->Set(0, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(1, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  result->Set(2, std::make_unique<InterpolableNumber>(segment.R1()));
  result->Set(3, std::make_unique<InterpolableNumber>(segment.R2()));
  result->Set(4, std::make_unique<InterpolableNumber>(segment.ArcAngle()));
  // TODO(alancutter): Make these flags part of the NonInterpolableValue.
  result->Set(5, std::make_unique<InterpolableNumber>(segment.LargeArcFlag()));
  result->Set(6, std::make_unique<InterpolableNumber>(segment.SweepFlag()));
  return std::move(result);
}

PathSegmentData ConsumeInterpolableArc(const InterpolableValue& value,
                                       SVGPathSegType seg_type,
                                       PathCoordinates& coordinates) {
  const InterpolableList& list = ToInterpolableList(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      list.Get(0), is_absolute, coordinates.current_x));
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      list.Get(1), is_absolute, coordinates.current_y));
  segment.ArcRadii().SetX(ToInterpolableNumber(list.Get(2))->Value());
  segment.ArcRadii().SetY(ToInterpolableNumber(list.Get(3))->Value());
  segment.SetArcAngle(ToInterpolableNumber(list.Get(4))->Value());
  segment.arc_large = ToInterpolableNumber(list.Get(5))->Value() >= 0.5;
  segment.arc_sweep = ToInterpolableNumber(list.Get(6))->Value() >= 0.5;
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeLinetoHorizontal(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  return ConsumeCoordinateAxis(segment.X(), is_absolute, coordinates.current_x);
}

PathSegmentData ConsumeInterpolableLinetoHorizontal(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      &value, is_absolute, coordinates.current_x));
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeLinetoVertical(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  return ConsumeCoordinateAxis(segment.Y(), is_absolute, coordinates.current_y);
}

PathSegmentData ConsumeInterpolableLinetoVertical(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      &value, is_absolute, coordinates.current_y));
  return segment;
}

std::unique_ptr<InterpolableValue> ConsumeCurvetoCubicSmooth(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto result = std::make_unique<InterpolableList>(4);
  result->Set(
      0, ConsumeControlAxis(segment.X2(), is_absolute, coordinates.current_x));
  result->Set(
      1, ConsumeControlAxis(segment.Y2(), is_absolute, coordinates.current_y));
  result->Set(2, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(3, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  return std::move(result);
}

PathSegmentData ConsumeInterpolableCurvetoCubicSmooth(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  const InterpolableList& list = ToInterpolableList(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point2.SetX(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                     coordinates.current_x));
  segment.point2.SetY(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                     coordinates.current_y));
  segment.target_point.SetX(ConsumeInterpolableCoordinateAxis(
      list.Get(2), is_absolute, coordinates.current_x));
  segment.target_point.SetY(ConsumeInterpolableCoordinateAxis(
      list.Get(3), is_absolute, coordinates.current_y));
  return segment;
}

std::unique_ptr<InterpolableValue>
SVGPathSegInterpolationFunctions::ConsumePathSeg(const PathSegmentData& segment,
                                                 PathCoordinates& coordinates) {
  switch (segment.command) {
    case kPathSegClosePath:
      return ConsumeClosePath(segment, coordinates);

    case kPathSegMoveToAbs:
    case kPathSegMoveToRel:
    case kPathSegLineToAbs:
    case kPathSegLineToRel:
    case kPathSegCurveToQuadraticSmoothAbs:
    case kPathSegCurveToQuadraticSmoothRel:
      return ConsumeSingleCoordinate(segment, coordinates);

    case kPathSegCurveToCubicAbs:
    case kPathSegCurveToCubicRel:
      return ConsumeCurvetoCubic(segment, coordinates);

    case kPathSegCurveToQuadraticAbs:
    case kPathSegCurveToQuadraticRel:
      return ConsumeCurvetoQuadratic(segment, coordinates);

    case kPathSegArcAbs:
    case kPathSegArcRel:
      return ConsumeArc(segment, coordinates);

    case kPathSegLineToHorizontalAbs:
    case kPathSegLineToHorizontalRel:
      return ConsumeLinetoHorizontal(segment, coordinates);

    case kPathSegLineToVerticalAbs:
    case kPathSegLineToVerticalRel:
      return ConsumeLinetoVertical(segment, coordinates);

    case kPathSegCurveToCubicSmoothAbs:
    case kPathSegCurveToCubicSmoothRel:
      return ConsumeCurvetoCubicSmooth(segment, coordinates);

    case kPathSegUnknown:
    default:
      NOTREACHED();
      return nullptr;
  }
}

PathSegmentData SVGPathSegInterpolationFunctions::ConsumeInterpolablePathSeg(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  switch (seg_type) {
    case kPathSegClosePath:
      return ConsumeInterpolableClosePath(value, seg_type, coordinates);

    case kPathSegMoveToAbs:
    case kPathSegMoveToRel:
    case kPathSegLineToAbs:
    case kPathSegLineToRel:
    case kPathSegCurveToQuadraticSmoothAbs:
    case kPathSegCurveToQuadraticSmoothRel:
      return ConsumeInterpolableSingleCoordinate(value, seg_type, coordinates);

    case kPathSegCurveToCubicAbs:
    case kPathSegCurveToCubicRel:
      return ConsumeInterpolableCurvetoCubic(value, seg_type, coordinates);

    case kPathSegCurveToQuadraticAbs:
    case kPathSegCurveToQuadraticRel:
      return ConsumeInterpolableCurvetoQuadratic(value, seg_type, coordinates);

    case kPathSegArcAbs:
    case kPathSegArcRel:
      return ConsumeInterpolableArc(value, seg_type, coordinates);

    case kPathSegLineToHorizontalAbs:
    case kPathSegLineToHorizontalRel:
      return ConsumeInterpolableLinetoHorizontal(value, seg_type, coordinates);

    case kPathSegLineToVerticalAbs:
    case kPathSegLineToVerticalRel:
      return ConsumeInterpolableLinetoVertical(value, seg_type, coordinates);

    case kPathSegCurveToCubicSmoothAbs:
    case kPathSegCurveToCubicSmoothRel:
      return ConsumeInterpolableCurvetoCubicSmooth(value, seg_type,
                                                   coordinates);

    case kPathSegUnknown:
    default:
      NOTREACHED();
      return PathSegmentData();
  }
}

}  // namespace blink
