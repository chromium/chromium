// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_path_seg_interpolation_functions.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

InterpolableNumber* ConsumeControlAxis(double value,
                                       bool is_absolute,
                                       double current_value) {
  return MakeGarbageCollected<InterpolableNumber>(
      is_absolute ? value : current_value + value);
}

float ConsumeInterpolableControlAxis(const InterpolableValue* number,
                                     bool is_absolute,
                                     double current_value) {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double value =
      To<InterpolableNumber>(number)->Value(CSSToLengthConversionData());
  return ClampTo<float>(is_absolute ? value : value - current_value);
}

InterpolableNumber* ConsumeCoordinateAxis(double value,
                                          bool is_absolute,
                                          double& current_value) {
  if (is_absolute) {
    current_value = value;
  } else {
    current_value += value;
  }
  return MakeGarbageCollected<InterpolableNumber>(current_value);
}

float ConsumeInterpolableCoordinateAxis(const InterpolableValue* number,
                                        bool is_absolute,
                                        double& current_value) {
  double previous_value = current_value;
  current_value =
      To<InterpolableNumber>(number)->Value(CSSToLengthConversionData());
  return ClampTo<float>(is_absolute ? current_value
                                    : current_value - previous_value);
}

InterpolableValue* ConsumeClosePath(const PathSegmentData&,
                                    PathCoordinates& coordinates) {
  coordinates.current_x = coordinates.initial_x;
  coordinates.current_y = coordinates.initial_y;
  return MakeGarbageCollected<InterpolableList>(0);
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

InterpolableValue* ConsumeSingleCoordinate(const PathSegmentData& segment,
                                           PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto* result = MakeGarbageCollected<InterpolableList>(2);
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

  return result;
}

PathSegmentData ConsumeInterpolableSingleCoordinate(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  const auto& list = To<InterpolableList>(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      list.Get(0), is_absolute, coordinates.current_x));
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      list.Get(1), is_absolute, coordinates.current_y));

  if (ToAbsolutePathSegType(seg_type) == kPathSegMoveToAbs) {
    // Any upcoming 'closepath' commands bring us back to the location we have
    // just moved to.
    coordinates.initial_x = coordinates.current_x;
    coordinates.initial_y = coordinates.current_y;
  }

  return segment;
}

InterpolableValue* ConsumeCurvetoCubic(const PathSegmentData& segment,
                                       PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto* result = MakeGarbageCollected<InterpolableList>(6);
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
  return result;
}

PathSegmentData ConsumeInterpolableCurvetoCubic(const InterpolableValue& value,
                                                SVGPathSegType seg_type,
                                                PathCoordinates& coordinates) {
  const auto& list = To<InterpolableList>(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point1.set_x(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                      coordinates.current_x));
  segment.point1.set_y(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                      coordinates.current_y));
  segment.point2.set_x(ConsumeInterpolableControlAxis(list.Get(2), is_absolute,
                                                      coordinates.current_x));
  segment.point2.set_y(ConsumeInterpolableControlAxis(list.Get(3), is_absolute,
                                                      coordinates.current_y));
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      list.Get(4), is_absolute, coordinates.current_x));
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      list.Get(5), is_absolute, coordinates.current_y));
  return segment;
}

InterpolableValue* ConsumeCurvetoQuadratic(const PathSegmentData& segment,
                                           PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto* result = MakeGarbageCollected<InterpolableList>(4);
  result->Set(
      0, ConsumeControlAxis(segment.X1(), is_absolute, coordinates.current_x));
  result->Set(
      1, ConsumeControlAxis(segment.Y1(), is_absolute, coordinates.current_y));
  result->Set(2, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(3, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  return result;
}

PathSegmentData ConsumeInterpolableCurvetoQuadratic(
    const InterpolableValue& value,
    SVGPathSegType seg_type,
    PathCoordinates& coordinates) {
  const auto& list = To<InterpolableList>(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point1.set_x(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                      coordinates.current_x));
  segment.point1.set_y(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                      coordinates.current_y));
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      list.Get(2), is_absolute, coordinates.current_x));
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      list.Get(3), is_absolute, coordinates.current_y));
  return segment;
}

InterpolableValue* ConsumeArc(const PathSegmentData& segment,
                              PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto* result = MakeGarbageCollected<InterpolableList>(7);
  result->Set(0, ConsumeCoordinateAxis(segment.X(), is_absolute,
                                       coordinates.current_x));
  result->Set(1, ConsumeCoordinateAxis(segment.Y(), is_absolute,
                                       coordinates.current_y));
  result->Set(2,
              MakeGarbageCollected<InterpolableNumber>(segment.ArcRadiusX()));
  result->Set(3,
              MakeGarbageCollected<InterpolableNumber>(segment.ArcRadiusY()));
  result->Set(4, MakeGarbageCollected<InterpolableNumber>(segment.ArcAngle()));
  // TODO(alancutter): Make these flags part of the NonInterpolableValue.
  result->Set(5,
              MakeGarbageCollected<InterpolableNumber>(segment.LargeArcFlag()));
  result->Set(6, MakeGarbageCollected<InterpolableNumber>(segment.SweepFlag()));
  return result;
}

PathSegmentData ConsumeInterpolableArc(const InterpolableValue& value,
                                       SVGPathSegType seg_type,
                                       PathCoordinates& coordinates) {
  const auto& list = To<InterpolableList>(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      list.Get(0), is_absolute, coordinates.current_x));
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      list.Get(1), is_absolute, coordinates.current_y));
  CSSToLengthConversionData length_resolver;
  segment.SetArcRadiusX(
      To<InterpolableNumber>(list.Get(2))->Value(length_resolver));
  segment.SetArcRadiusY(
      To<InterpolableNumber>(list.Get(3))->Value(length_resolver));
  segment.SetArcAngle(
      To<InterpolableNumber>(list.Get(4))->Value(length_resolver));
  segment.arc_large =
      To<InterpolableNumber>(list.Get(5))->Value(length_resolver) >= 0.5;
  segment.arc_sweep =
      To<InterpolableNumber>(list.Get(6))->Value(length_resolver) >= 0.5;
  return segment;
}

InterpolableValue* ConsumeLinetoHorizontal(const PathSegmentData& segment,
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
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      &value, is_absolute, coordinates.current_x));
  return segment;
}

InterpolableValue* ConsumeLinetoVertical(const PathSegmentData& segment,
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
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      &value, is_absolute, coordinates.current_y));
  return segment;
}

InterpolableValue* ConsumeCurvetoCubicSmooth(const PathSegmentData& segment,
                                             PathCoordinates& coordinates) {
  bool is_absolute = IsAbsolutePathSegType(segment.command);
  auto* result = MakeGarbageCollected<InterpolableList>(4);
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
  const auto& list = To<InterpolableList>(value);
  bool is_absolute = IsAbsolutePathSegType(seg_type);
  PathSegmentData segment;
  segment.command = seg_type;
  segment.point2.set_x(ConsumeInterpolableControlAxis(list.Get(0), is_absolute,
                                                      coordinates.current_x));
  segment.point2.set_y(ConsumeInterpolableControlAxis(list.Get(1), is_absolute,
                                                      coordinates.current_y));
  segment.target_point.set_x(ConsumeInterpolableCoordinateAxis(
      list.Get(2), is_absolute, coordinates.current_x));
  segment.target_point.set_y(ConsumeInterpolableCoordinateAxis(
      list.Get(3), is_absolute, coordinates.current_y));
  return segment;
}

InterpolableValue* SVGPathSegInterpolationFunctions::ConsumePathSeg(
    const PathSegmentData& segment,
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return PathSegmentData();
  }
}

}  // namespace blink
