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

#include "third_party/blink/renderer/core/css/basic_shape_functions.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_shape_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/shape_functions.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_ray.h"
#include "third_party/blink/renderer/core/style/style_shape.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

static StyleRay::RaySize KeywordToRaySize(CSSValueID id) {
  switch (id) {
    case CSSValueID::kClosestSide:
      return StyleRay::RaySize::kClosestSide;
    case CSSValueID::kClosestCorner:
      return StyleRay::RaySize::kClosestCorner;
    case CSSValueID::kFarthestSide:
      return StyleRay::RaySize::kFarthestSide;
    case CSSValueID::kFarthestCorner:
      return StyleRay::RaySize::kFarthestCorner;
    case CSSValueID::kSides:
      return StyleRay::RaySize::kSides;
    default:
      NOTREACHED();
  }
}

static CSSValueID RaySizeToKeyword(StyleRay::RaySize size) {
  switch (size) {
    case StyleRay::RaySize::kClosestSide:
      return CSSValueID::kClosestSide;
    case StyleRay::RaySize::kClosestCorner:
      return CSSValueID::kClosestCorner;
    case StyleRay::RaySize::kFarthestSide:
      return CSSValueID::kFarthestSide;
    case StyleRay::RaySize::kFarthestCorner:
      return CSSValueID::kFarthestCorner;
    case StyleRay::RaySize::kSides:
      return CSSValueID::kSides;
  }
  NOTREACHED();
}

const CSSValuePair& LengthPointToCSSValue(const LengthPoint& point,
                                          float zoom) {
  return *MakeGarbageCollected<CSSValuePair>(
      CSSPrimitiveValue::CreateFromLength(point.X(), zoom),
      CSSPrimitiveValue::CreateFromLength(point.Y(), zoom),
      CSSValuePair::IdenticalValuesPolicy::kKeepIdenticalValues);
}

template <typename T, wtf_size_t NumControlPoints = T::GetNumControlPoints()>
StyleShape::Segment CurveCommandToShapeSegment(
    const cssvalue::CSSShapeCommand& command,
    const StyleResolverState& state) {
  const auto& curve =
      static_cast<const cssvalue::CSSShapeCurveCommand<NumControlPoints>&>(
          command);
  std::array<StyleShape::ControlPoint, NumControlPoints> control_points;

  std::ranges::transform(curve.GetControlPoints(), control_points.begin(),
                         [&](const cssvalue::CSSShapeControlPoint& value) {
                           return StyleShape::ControlPoint{
                               .origin = ToControlPointOrigin(value.first),
                               .point = StyleBuilderConverter::ConvertPosition(
                                   state, *value.second)};
                         });

  return T{
      {{StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())},
       std::move(control_points)}};
}

StyleShape::Segment ShapeCommandToShapeSegment(
    const cssvalue::CSSShapeCommand& command,
    const StyleResolverState& state) {
  switch (command.GetType()) {
    case SVGPathSegType::kPathSegMoveToAbs:
      return StyleShape::MoveToSegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegMoveToRel:
      return StyleShape::MoveBySegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToAbs:
      return StyleShape::LineToSegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToRel:
      return StyleShape::LineBySegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToHorizontalAbs:
      if (auto* identifier_value =
              DynamicTo<CSSIdentifierValue>(command.GetEndPoint())) {
        if (identifier_value->GetValueID() == CSSValueID::kXStart) {
          return StyleShape::HLineToSegment{Length::Percent(0)};
        } else if (identifier_value->GetValueID() == CSSValueID::kXEnd) {
          return StyleShape::HLineToSegment{Length::Percent(100)};
        }
      }
      return StyleShape::HLineToSegment{
          StyleBuilderConverter::ConvertPositionLength<CSSValueID::kLeft,
                                                       CSSValueID::kRight>(
              state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToHorizontalRel:
      return StyleShape::HLineBySegment{
          StyleBuilderConverter::ConvertPositionLength<CSSValueID::kLeft,
                                                       CSSValueID::kRight>(
              state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToVerticalAbs:
      if (auto* identifier_value =
              DynamicTo<CSSIdentifierValue>(command.GetEndPoint())) {
        if (identifier_value->GetValueID() == CSSValueID::kYStart) {
          return StyleShape::VLineToSegment{Length::Percent(0)};
        } else if (identifier_value->GetValueID() == CSSValueID::kYEnd) {
          return StyleShape::VLineToSegment{Length::Percent(100)};
        }
      }
      return StyleShape::VLineToSegment{
          StyleBuilderConverter::ConvertPositionLength<CSSValueID::kTop,
                                                       CSSValueID::kBottom>(
              state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegLineToVerticalRel:
      return StyleShape::VLineBySegment{
          StyleBuilderConverter::ConvertPositionLength<CSSValueID::kTop,
                                                       CSSValueID::kBottom>(
              state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegCurveToCubicAbs:
      return CurveCommandToShapeSegment<StyleShape::CubicCurveToSegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToCubicRel:
      return CurveCommandToShapeSegment<StyleShape::CubicCurveBySegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToQuadraticAbs:
      return CurveCommandToShapeSegment<StyleShape::QuadraticCurveToSegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToQuadraticRel:
      return CurveCommandToShapeSegment<StyleShape::QuadraticCurveBySegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToCubicSmoothAbs:
      return CurveCommandToShapeSegment<StyleShape::SmoothCubicCurveToSegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToCubicSmoothRel:
      return CurveCommandToShapeSegment<StyleShape::SmoothCubicCurveBySegment>(
          command, state);
    case SVGPathSegType::kPathSegCurveToQuadraticSmoothAbs:
      return StyleShape::SmoothQuadraticCurveToSegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegCurveToQuadraticSmoothRel:
      return StyleShape::SmoothQuadraticCurveBySegment{
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint())};
    case SVGPathSegType::kPathSegArcAbs:
    case SVGPathSegType::kPathSegArcRel: {
      const cssvalue::CSSShapeArcCommand& arc =
          static_cast<const cssvalue::CSSShapeArcCommand&>(command);

      LengthPoint target_point =
          StyleBuilderConverter::ConvertPosition(state, command.GetEndPoint());

      float angle =
          arc.Angle().ComputeDegrees(state.CssToLengthConversionData());
      const Length zero_length = Length::Fixed(0);
      LengthSize radius =
          arc.HasDirectionAgnosticRadius()
              ? LengthSize(zero_length, zero_length)
              : StyleBuilderConverter::ConvertRadius(state, arc.Radius());
      Length direction_agnostic_radius =
          arc.HasDirectionAgnosticRadius()
              ? StyleBuilderConverter::ConvertLength(state,
                                                     arc.Radius().First())
              : zero_length;
      bool large = arc.Size() == CSSValueID::kLarge;
      bool sweep = arc.Sweep() == CSSValueID::kCw;
      return command.GetType() == SVGPathSegType::kPathSegArcAbs
                 ? StyleShape::Segment(
                       StyleShape::ArcToSegment{{{target_point},
                                                 angle,
                                                 radius,
                                                 direction_agnostic_radius,
                                                 large,
                                                 sweep}})
                 : StyleShape::Segment(
                       StyleShape::ArcBySegment{{{target_point},
                                                 angle,
                                                 radius,
                                                 direction_agnostic_radius,
                                                 large,
                                                 sweep}});
    }
    case SVGPathSegType::kPathSegClosePath:
      return StyleShape::CloseSegment{};
    case SVGPathSegType::kPathSegUnknown:
      NOTREACHED();
  }
}

struct ShapeSegmentToShapeCommandVisitor {
  // TODO(crbug.com/384870259): support curve/smooth.

  using CSSShapeCommand = cssvalue::CSSShapeCommand;

  const CSSShapeCommand* operator()(const StyleShape::MoveToSegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::MoveBySegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::LineToSegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::LineBySegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::HLineToSegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType,
        *CSSPrimitiveValue::CreateFromLength(segment.x, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::HLineBySegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType,
        *CSSPrimitiveValue::CreateFromLength(segment.x, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::VLineToSegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType,
        *CSSPrimitiveValue::CreateFromLength(segment.y, zoom));
  }
  const CSSShapeCommand* operator()(const StyleShape::VLineBySegment& segment) {
    return MakeGarbageCollected<const CSSShapeCommand>(
        segment.kSegType,
        *CSSPrimitiveValue::CreateFromLength(segment.y, zoom));
  }

  const CSSShapeCommand* operator()(
      const StyleShape::CubicCurveToSegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)),
        ToControlPoint(segment.control_points.at(1)));
  }

  const CSSShapeCommand* operator()(
      const StyleShape::CubicCurveBySegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)),
        ToControlPoint(segment.control_points.at(1)));
  }

  const CSSShapeCommand* operator()(
      const StyleShape::QuadraticCurveToSegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)));
  }
  const CSSShapeCommand* operator()(
      const StyleShape::QuadraticCurveBySegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)));
  }
  const CSSShapeCommand* operator()(
      const StyleShape::SmoothCubicCurveToSegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)));
  }

  const CSSShapeCommand* operator()(
      const StyleShape::SmoothCubicCurveBySegment& segment) {
    return MakeGarbageCollected<cssvalue::CSSShapeCurveCommand<2>>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom),
        ToControlPoint(segment.control_points.at(0)));
  }

  const CSSShapeCommand* operator()(
      const StyleShape::SmoothQuadraticCurveToSegment& segment) {
    return MakeGarbageCollected<const cssvalue::CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }
  const CSSShapeCommand* operator()(
      const StyleShape::SmoothQuadraticCurveBySegment& segment) {
    return MakeGarbageCollected<const cssvalue::CSSShapeCommand>(
        segment.kSegType, LengthPointToCSSValue(segment.target_point, zoom));
  }

  const CSSShapeCommand* operator()(const StyleShape::ArcToSegment& segment) {
    return Arc(segment.kSegType, segment);
  }
  const CSSShapeCommand* operator()(const StyleShape::ArcBySegment& segment) {
    return Arc(segment.kSegType, segment);
  }

  const CSSShapeCommand* operator()(const StyleShape::CloseSegment&) {
    return CSSShapeCommand::Close();
  }

  const cssvalue::CSSShapeControlPoint ToControlPoint(
      const StyleShape::ControlPoint& control_point) {
    return cssvalue::CSSShapeControlPoint(
        FromControlPointOrigin(control_point.origin),
        LengthPointToCSSValue(control_point.point, zoom));
  }

  template <SVGPathSegType T>
  const CSSShapeCommand* Arc(CSSShapeCommand::Type type,
                             const StyleShape::ArcSegment<T>& segment) {
    const bool treat_as_direction_agnostic_radius =
        segment.radius.Width().IsZero() && segment.radius.Height().IsZero();
    return MakeGarbageCollected<const cssvalue::CSSShapeArcCommand>(
        type, LengthPointToCSSValue(segment.target_point, zoom),
        *CSSNumericLiteralValue::Create(segment.angle,
                                        CSSPrimitiveValue::UnitType::kDegrees),
        *MakeGarbageCollected<CSSValuePair>(
            CSSPrimitiveValue::CreateFromLength(
                treat_as_direction_agnostic_radius
                    ? segment.direction_agnostic_radius
                    : segment.radius.Width(),
                zoom),
            CSSPrimitiveValue::CreateFromLength(
                treat_as_direction_agnostic_radius
                    ? segment.direction_agnostic_radius
                    : segment.radius.Height(),
                zoom),
            treat_as_direction_agnostic_radius
                ? CSSValuePair::IdenticalValuesPolicy::kDropIdenticalValues
                : CSSValuePair::IdenticalValuesPolicy::kKeepIdenticalValues),
        segment.large ? CSSValueID::kLarge : CSSValueID::kSmall,
        segment.sweep ? CSSValueID::kCw : CSSValueID::kCcw,
        treat_as_direction_agnostic_radius);
  }

  float zoom;
};

static CSSValue* ValueForCenterCoordinate(
    const ComputedStyle& style,
    const BasicShapeCenterCoordinate& center,
    EBoxOrient orientation) {
  if (center.GetDirection() == BasicShapeCenterCoordinate::kTopLeft) {
    return CSSValue::Create(center.length(), style.EffectiveZoom());
  }

  CSSValueID keyword = orientation == EBoxOrient::kHorizontal
                           ? CSSValueID::kRight
                           : CSSValueID::kBottom;

  return MakeGarbageCollected<CSSValuePair>(
      CSSIdentifierValue::Create(keyword),
      CSSValue::Create(center.length(), style.EffectiveZoom()),
      CSSValuePair::kDropIdenticalValues);
}

static CSSValuePair* ValueForLengthSize(const LengthSize& length_size,
                                        const ComputedStyle& style) {
  return MakeGarbageCollected<CSSValuePair>(
      CSSValue::Create(length_size.Width(), style.EffectiveZoom()),
      CSSValue::Create(length_size.Height(), style.EffectiveZoom()),
      CSSValuePair::kKeepIdenticalValues);
}

static CSSValue* BasicShapeRadiusToCSSValue(const ComputedStyle& style,
                                            const BasicShapeRadius& radius) {
  switch (radius.GetType()) {
    case BasicShapeRadius::kValue:
      return CSSValue::Create(radius.Value(), style.EffectiveZoom());
    case BasicShapeRadius::kClosestSide:
      return CSSIdentifierValue::Create(CSSValueID::kClosestSide);
    case BasicShapeRadius::kFarthestSide:
      return CSSIdentifierValue::Create(CSSValueID::kFarthestSide);
  }

  NOTREACHED();
}

template <typename BasicShapeClass, typename CSSValueClass>
static void InitializeBorderRadius(BasicShapeClass* rect,
                                   const StyleResolverState& state,
                                   const CSSValueClass& rect_value) {
  rect->SetTopLeftRadius(
      ConvertToLengthSize(state, rect_value.TopLeftRadius()));
  rect->SetTopRightRadius(
      ConvertToLengthSize(state, rect_value.TopRightRadius()));
  rect->SetBottomRightRadius(
      ConvertToLengthSize(state, rect_value.BottomRightRadius()));
  rect->SetBottomLeftRadius(
      ConvertToLengthSize(state, rect_value.BottomLeftRadius()));
}

template <typename BasicShapeClass, typename CSSValueClass>
static void InitializeBorderRadius(CSSValueClass* css_value,
                                   const ComputedStyle& style,
                                   const BasicShapeClass* rect) {
  css_value->SetTopLeftRadius(ValueForLengthSize(rect->TopLeftRadius(), style));
  css_value->SetTopRightRadius(
      ValueForLengthSize(rect->TopRightRadius(), style));
  css_value->SetBottomRightRadius(
      ValueForLengthSize(rect->BottomRightRadius(), style));
  css_value->SetBottomLeftRadius(
      ValueForLengthSize(rect->BottomLeftRadius(), style));
}

CSSValue* ValueForBasicShape(const ComputedStyle& style,
                             const BasicShape* basic_shape) {
  switch (basic_shape->GetType()) {
    case BasicShape::kStyleRayType: {
      const StyleRay& ray = To<StyleRay>(*basic_shape);
      const CSSValue* center_x =
          ray.HasExplicitCenter()
              ? ValueForCenterCoordinate(style, ray.CenterX(),
                                         EBoxOrient::kHorizontal)
              : nullptr;
      const CSSValue* center_y =
          ray.HasExplicitCenter()
              ? ValueForCenterCoordinate(style, ray.CenterY(),
                                         EBoxOrient::kVertical)
              : nullptr;
      return MakeGarbageCollected<cssvalue::CSSRayValue>(
          *CSSNumericLiteralValue::Create(
              ray.Angle(), CSSPrimitiveValue::UnitType::kDegrees),
          *CSSIdentifierValue::Create(RaySizeToKeyword(ray.Size())),
          (ray.Contain() ? CSSIdentifierValue::Create(CSSValueID::kContain)
                         : nullptr),
          center_x, center_y);
    }

    case BasicShape::kStylePathType:
      return To<StylePath>(basic_shape)->ComputedCSSValue();

    case BasicShape::kStyleShapeType: {
      const StyleShape* shape = To<StyleShape>(basic_shape);
      HeapVector<Member<const cssvalue::CSSShapeCommand>> commands;
      commands.reserve(shape->Segments().size());
      ShapeSegmentToShapeCommandVisitor visitor{style.EffectiveZoom()};
      for (const auto& segment : shape->Segments()) {
        commands.push_back(std::visit(visitor, segment));
      }
      return MakeGarbageCollected<cssvalue::CSSShapeValue>(
          shape->GetWindRule(),
          LengthPointToCSSValue(shape->GetOrigin(), style.EffectiveZoom()),
          std::move(commands));
    }

    case BasicShape::kBasicShapeCircleType: {
      const BasicShapeCircle* circle = To<BasicShapeCircle>(basic_shape);
      cssvalue::CSSBasicShapeCircleValue* circle_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapeCircleValue>();

      if (circle->HasExplicitCenter()) {
        circle_value->SetCenterX(ValueForCenterCoordinate(
            style, circle->CenterX(), EBoxOrient::kHorizontal));
        circle_value->SetCenterY(ValueForCenterCoordinate(
            style, circle->CenterY(), EBoxOrient::kVertical));
      }
      circle_value->SetRadius(
          BasicShapeRadiusToCSSValue(style, circle->Radius()));
      return circle_value;
    }
    case BasicShape::kBasicShapeEllipseType: {
      const BasicShapeEllipse* ellipse = To<BasicShapeEllipse>(basic_shape);
      auto* ellipse_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapeEllipseValue>();

      if (ellipse->HasExplicitCenter()) {
        ellipse_value->SetCenterX(ValueForCenterCoordinate(
            style, ellipse->CenterX(), EBoxOrient::kHorizontal));
        ellipse_value->SetCenterY(ValueForCenterCoordinate(
            style, ellipse->CenterY(), EBoxOrient::kVertical));
      }
      ellipse_value->SetRadiusX(
          BasicShapeRadiusToCSSValue(style, ellipse->RadiusX()));
      ellipse_value->SetRadiusY(
          BasicShapeRadiusToCSSValue(style, ellipse->RadiusY()));
      return ellipse_value;
    }
    case BasicShape::kBasicShapePolygonType: {
      const BasicShapePolygon* polygon = To<BasicShapePolygon>(basic_shape);
      auto* polygon_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapePolygonValue>();

      polygon_value->SetWindRule(polygon->GetWindRule());
      const Vector<Length>& values = polygon->Values();
      for (unsigned i = 0; i < values.size(); i += 2) {
        polygon_value->AppendPoint(
            CSSPrimitiveValue::CreateFromLength(values.at(i),
                                                style.EffectiveZoom()),
            CSSPrimitiveValue::CreateFromLength(values.at(i + 1),
                                                style.EffectiveZoom()));
      }
      return polygon_value;
    }
    case BasicShape::kBasicShapeInsetType: {
      const BasicShapeInset* inset = To<BasicShapeInset>(basic_shape);
      cssvalue::CSSBasicShapeInsetValue* inset_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapeInsetValue>();

      inset_value->SetTop(CSSPrimitiveValue::CreateFromLength(
          inset->Top(), style.EffectiveZoom()));
      inset_value->SetRight(CSSPrimitiveValue::CreateFromLength(
          inset->Right(), style.EffectiveZoom()));
      inset_value->SetBottom(CSSPrimitiveValue::CreateFromLength(
          inset->Bottom(), style.EffectiveZoom()));
      inset_value->SetLeft(CSSPrimitiveValue::CreateFromLength(
          inset->Left(), style.EffectiveZoom()));

      InitializeBorderRadius(inset_value, style, inset);
      return inset_value;
    }
    default:
      return nullptr;
  }
}

static Length ConvertToLength(const StyleResolverState& state,
                              const CSSPrimitiveValue* value) {
  if (!value) {
    return Length::Fixed(0);
  }
  return value->ConvertToLength(state.CssToLengthConversionData());
}

static LengthSize ConvertToLengthSize(const StyleResolverState& state,
                                      const CSSValuePair* value) {
  if (!value) {
    return LengthSize(Length::Fixed(0), Length::Fixed(0));
  }

  return LengthSize(
      ConvertToLength(state, &To<CSSPrimitiveValue>(value->First())),
      ConvertToLength(state, &To<CSSPrimitiveValue>(value->Second())));
}

static BasicShapeCenterCoordinate ConvertToCenterCoordinate(
    const StyleResolverState& state,
    const CSSValue* value) {
  BasicShapeCenterCoordinate::Direction direction;
  Length offset = Length::Fixed(0);

  CSSValueID keyword = CSSValueID::kTop;
  if (!value) {
    keyword = CSSValueID::kCenter;
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    keyword = identifier_value->GetValueID();
  } else if (auto* value_pair = DynamicTo<CSSValuePair>(value)) {
    keyword = To<CSSIdentifierValue>(value_pair->First()).GetValueID();
    offset =
        ConvertToLength(state, &To<CSSPrimitiveValue>(value_pair->Second()));
  } else {
    offset = ConvertToLength(state, To<CSSPrimitiveValue>(value));
  }

  switch (keyword) {
    case CSSValueID::kTop:
    case CSSValueID::kLeft:
      direction = BasicShapeCenterCoordinate::kTopLeft;
      break;
    case CSSValueID::kRight:
    case CSSValueID::kBottom:
      direction = BasicShapeCenterCoordinate::kBottomRight;
      break;
    case CSSValueID::kCenter:
      direction = BasicShapeCenterCoordinate::kTopLeft;
      offset = Length::Percent(50);
      break;
    default:
      NOTREACHED();
  }

  return BasicShapeCenterCoordinate(direction, offset);
}

static BasicShapeRadius CssValueToBasicShapeRadius(
    const StyleResolverState& state,
    const CSSValue* radius) {
  if (!radius) {
    return BasicShapeRadius(BasicShapeRadius::kClosestSide);
  }

  if (auto* radius_identifier_value = DynamicTo<CSSIdentifierValue>(radius)) {
    switch (radius_identifier_value->GetValueID()) {
      case CSSValueID::kClosestSide:
        return BasicShapeRadius(BasicShapeRadius::kClosestSide);
      case CSSValueID::kFarthestSide:
        return BasicShapeRadius(BasicShapeRadius::kFarthestSide);
      default:
        NOTREACHED();
    }
  }

  return BasicShapeRadius(
      ConvertToLength(state, To<CSSPrimitiveValue>(radius)));
}

BasicShape* BasicShapeForValue(const StyleResolverState& state,
                               const CSSValue& basic_shape_value) {
  if (const auto* circle_value =
          DynamicTo<cssvalue::CSSBasicShapeCircleValue>(basic_shape_value)) {
    BasicShapeCircle* circle = MakeGarbageCollected<BasicShapeCircle>();

    circle->SetCenterX(
        ConvertToCenterCoordinate(state, circle_value->CenterX()));
    circle->SetCenterY(
        ConvertToCenterCoordinate(state, circle_value->CenterY()));
    circle->SetRadius(
        CssValueToBasicShapeRadius(state, circle_value->Radius()));
    circle->SetHasExplicitCenter(circle_value->CenterX());

    return circle;
  } else if (const auto* ellipse_value =
                 DynamicTo<cssvalue::CSSBasicShapeEllipseValue>(
                     basic_shape_value)) {
    BasicShapeEllipse* ellipse = MakeGarbageCollected<BasicShapeEllipse>();

    ellipse->SetCenterX(
        ConvertToCenterCoordinate(state, ellipse_value->CenterX()));
    ellipse->SetCenterY(
        ConvertToCenterCoordinate(state, ellipse_value->CenterY()));
    ellipse->SetRadiusX(
        CssValueToBasicShapeRadius(state, ellipse_value->RadiusX()));
    ellipse->SetRadiusY(
        CssValueToBasicShapeRadius(state, ellipse_value->RadiusY()));
    ellipse->SetHasExplicitCenter(ellipse_value->CenterX());

    return ellipse;
  } else if (const auto* polygon_value =
                 DynamicTo<cssvalue::CSSBasicShapePolygonValue>(
                     basic_shape_value)) {
    BasicShapePolygon* polygon = MakeGarbageCollected<BasicShapePolygon>();

    polygon->SetWindRule(polygon_value->GetWindRule());
    const HeapVector<Member<CSSPrimitiveValue>>& values =
        polygon_value->Values();
    for (unsigned i = 0; i < values.size(); i += 2) {
      polygon->AppendPoint(ConvertToLength(state, values.at(i).Get()),
                           ConvertToLength(state, values.at(i + 1).Get()));
    }

    return polygon;
  } else if (const auto* inset_value =
                 DynamicTo<cssvalue::CSSBasicShapeInsetValue>(
                     basic_shape_value)) {
    BasicShapeInset* rect = MakeGarbageCollected<BasicShapeInset>();

    rect->SetTop(
        ConvertToLength(state, To<CSSPrimitiveValue>(inset_value->Top())));
    rect->SetRight(
        ConvertToLength(state, To<CSSPrimitiveValue>(inset_value->Right())));
    rect->SetBottom(
        ConvertToLength(state, To<CSSPrimitiveValue>(inset_value->Bottom())));
    rect->SetLeft(
        ConvertToLength(state, To<CSSPrimitiveValue>(inset_value->Left())));

    InitializeBorderRadius(rect, state, *inset_value);
    return rect;
  } else if (const auto* rect_value =
                 DynamicTo<cssvalue::CSSBasicShapeRectValue>(
                     basic_shape_value)) {
    BasicShapeInset* inset = MakeGarbageCollected<BasicShapeInset>();

    // Spec: All <basic-shape-rect> functions compute to the equivalent
    // inset() function. NOTE: Given `rect(t r b l)`, the equivalent function
    // is `inset(t calc(100% - r) calc(100% - b) l)`.
    // See: https://drafts.csswg.org/css-shapes/#basic-shape-computed-values
    auto get_inset_length = [&](const CSSValue& edge,
                                bool is_right_or_bottom) -> Length {
      // Auto values coincide with the corresponding edge of the reference
      // box (https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-rect),
      // so the inset of any auto value will be 0.
      if (auto* auto_value = DynamicTo<CSSIdentifierValue>(edge)) {
        DCHECK_EQ(auto_value->GetValueID(), CSSValueID::kAuto);
        return Length::Percent(0);
      }
      Length edge_length = ConvertToLength(state, &To<CSSPrimitiveValue>(edge));
      return is_right_or_bottom ? edge_length.SubtractFromOneHundredPercent()
                                : edge_length;
    };
    inset->SetTop(get_inset_length(*rect_value->Top(), false));
    inset->SetRight(get_inset_length(*rect_value->Right(), true));
    inset->SetBottom(get_inset_length(*rect_value->Bottom(), true));
    inset->SetLeft(get_inset_length(*rect_value->Left(), false));

    InitializeBorderRadius(inset, state, *rect_value);
    return inset;
  } else if (const auto* xywh_value =
                 DynamicTo<cssvalue::CSSBasicShapeXYWHValue>(
                     basic_shape_value)) {
    BasicShapeInset* inset = MakeGarbageCollected<BasicShapeInset>();

    // Spec: All <basic-shape-rect> functions compute to the equivalent
    // inset() function. NOTE: Given `xywh(x y w h)`, the equivalent function
    // is `inset(y calc(100% - x - w) calc(100% - y - h) x)`.
    // See: https://drafts.csswg.org/css-shapes/#basic-shape-computed-values
    // and https://github.com/w3c/csswg-drafts/issues/9053
    inset->SetLeft(ConvertToLength(state, xywh_value->X()));
    // calc(100% - (x + w)) = calc(100% - x - w).
    inset->SetRight(inset->Left()
                        .Add(ConvertToLength(state, xywh_value->Width()))
                        .SubtractFromOneHundredPercent());
    inset->SetTop(ConvertToLength(state, xywh_value->Y()));
    // calc(100% - (y + h)) = calc(100% - y - h).
    inset->SetBottom(inset->Top()
                         .Add(ConvertToLength(state, xywh_value->Height()))
                         .SubtractFromOneHundredPercent());

    InitializeBorderRadius(inset, state, *xywh_value);
    return inset;
  } else if (const auto* ray_value =
                 DynamicTo<cssvalue::CSSRayValue>(basic_shape_value)) {
    float angle =
        ray_value->Angle().ComputeDegrees(state.CssToLengthConversionData());
    StyleRay::RaySize size = KeywordToRaySize(ray_value->Size().GetValueID());
    bool contain = !!ray_value->Contain();
    return MakeGarbageCollected<StyleRay>(
        angle, size, contain,
        ConvertToCenterCoordinate(state, ray_value->CenterX()),
        ConvertToCenterCoordinate(state, ray_value->CenterY()),
        ray_value->CenterX());
  } else if (const auto* path_value =
                 DynamicTo<cssvalue::CSSPathValue>(basic_shape_value)) {
    return path_value->GetStylePath();
  } else if (const auto* shape_value =
                 DynamicTo<cssvalue::CSSShapeValue>(basic_shape_value)) {
    Vector<StyleShape::Segment> segments;
    segments.ReserveInitialCapacity(shape_value->Commands().size());
    for (const cssvalue::CSSShapeCommand* command : shape_value->Commands()) {
      segments.push_back(ShapeCommandToShapeSegment(*command, state));
    }
    return MakeGarbageCollected<StyleShape>(
        shape_value->GetWindRule(),
        StyleBuilderConverter::ConvertPosition(state, shape_value->GetOrigin()),
        std::move(segments));
  }
  NOTREACHED();
}

}  // namespace blink
