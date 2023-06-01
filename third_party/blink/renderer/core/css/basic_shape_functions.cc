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
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_ray.h"
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
      return StyleRay::RaySize::kClosestSide;
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
  return CSSValueID::kInvalid;
}

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
  return nullptr;
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
    case BasicShape::kBasicShapeRectType: {
      const BasicShapeRect* rect = To<BasicShapeRect>(basic_shape);

      auto get_length = [&](const Length& length) -> CSSValue* {
        if (length.GetType() == Length::kAuto) {
          return CSSIdentifierValue::Create(CSSValueID::kAuto);
        }

        return CSSPrimitiveValue::CreateFromLength(length,
                                                   style.EffectiveZoom());
      };

      CSSValue* top = get_length(rect->Top());
      CSSValue* right = get_length(rect->Right());
      CSSValue* bottom = get_length(rect->Bottom());
      CSSValue* left = get_length(rect->Left());

      cssvalue::CSSBasicShapeRectValue* rect_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapeRectValue>(top, right,
                                                                 bottom, left);
      InitializeBorderRadius(rect_value, style, rect);
      return rect_value;
    }
    case BasicShape::kBasicShapeXYWHType: {
      const BasicShapeXYWH* rect = To<BasicShapeXYWH>(basic_shape);

      CSSValue* x =
          CSSPrimitiveValue::CreateFromLength(rect->X(), style.EffectiveZoom());
      CSSValue* y =
          CSSPrimitiveValue::CreateFromLength(rect->Y(), style.EffectiveZoom());
      CSSValue* width = CSSPrimitiveValue::CreateFromLength(
          rect->Width(), style.EffectiveZoom());
      CSSValue* height = CSSPrimitiveValue::CreateFromLength(
          rect->Height(), style.EffectiveZoom());

      cssvalue::CSSBasicShapeRectValue* rect_value =
          MakeGarbageCollected<cssvalue::CSSBasicShapeRectValue>(x, y, width,
                                                                 height);
      InitializeBorderRadius(rect_value, style, rect);
      return rect_value;
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
      direction = BasicShapeCenterCoordinate::kTopLeft;
      break;
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
        break;
    }
  }

  return BasicShapeRadius(
      ConvertToLength(state, To<CSSPrimitiveValue>(radius)));
}

scoped_refptr<BasicShape> BasicShapeForValue(
    const StyleResolverState& state,
    const CSSValue& basic_shape_value) {
  scoped_refptr<BasicShape> basic_shape;

  if (const auto* circle_value =
          DynamicTo<cssvalue::CSSBasicShapeCircleValue>(basic_shape_value)) {
    scoped_refptr<BasicShapeCircle> circle = BasicShapeCircle::Create();

    circle->SetCenterX(
        ConvertToCenterCoordinate(state, circle_value->CenterX()));
    circle->SetCenterY(
        ConvertToCenterCoordinate(state, circle_value->CenterY()));
    circle->SetRadius(
        CssValueToBasicShapeRadius(state, circle_value->Radius()));
    circle->SetHasExplicitCenter(circle_value->CenterX());

    basic_shape = std::move(circle);
  } else if (const auto* ellipse_value =
                 DynamicTo<cssvalue::CSSBasicShapeEllipseValue>(
                     basic_shape_value)) {
    scoped_refptr<BasicShapeEllipse> ellipse = BasicShapeEllipse::Create();

    ellipse->SetCenterX(
        ConvertToCenterCoordinate(state, ellipse_value->CenterX()));
    ellipse->SetCenterY(
        ConvertToCenterCoordinate(state, ellipse_value->CenterY()));
    ellipse->SetRadiusX(
        CssValueToBasicShapeRadius(state, ellipse_value->RadiusX()));
    ellipse->SetRadiusY(
        CssValueToBasicShapeRadius(state, ellipse_value->RadiusY()));
    ellipse->SetHasExplicitCenter(ellipse_value->CenterX());

    basic_shape = std::move(ellipse);
  } else if (const auto* polygon_value =
                 DynamicTo<cssvalue::CSSBasicShapePolygonValue>(
                     basic_shape_value)) {
    scoped_refptr<BasicShapePolygon> polygon = BasicShapePolygon::Create();

    polygon->SetWindRule(polygon_value->GetWindRule());
    const HeapVector<Member<CSSPrimitiveValue>>& values =
        polygon_value->Values();
    for (unsigned i = 0; i < values.size(); i += 2) {
      polygon->AppendPoint(ConvertToLength(state, values.at(i).Get()),
                           ConvertToLength(state, values.at(i + 1).Get()));
    }

    basic_shape = std::move(polygon);
  } else if (const auto* inset_value =
                 DynamicTo<cssvalue::CSSBasicShapeInsetValue>(
                     basic_shape_value)) {
    scoped_refptr<BasicShapeInset> rect = BasicShapeInset::Create();

    rect->SetTop(ConvertToLength(state, inset_value->Top()));
    rect->SetRight(ConvertToLength(state, inset_value->Right()));
    rect->SetBottom(ConvertToLength(state, inset_value->Bottom()));
    rect->SetLeft(ConvertToLength(state, inset_value->Left()));

    InitializeBorderRadius(rect.get(), state, *inset_value);
    basic_shape = std::move(rect);
  } else if (const auto* rect_value =
                 DynamicTo<cssvalue::CSSBasicShapeRectValue>(
                     basic_shape_value)) {
    scoped_refptr<BasicShapeRect> rect = BasicShapeRect::Create();

    auto get_length = [&](CSSValue* length) {
      if (length->IsIdentifierValue()) {
        auto* value = To<CSSIdentifierValue>(length);
        DCHECK_EQ(value->GetValueID(), CSSValueID::kAuto);
        return Length::Auto();
      }

      return ConvertToLength(state, To<CSSPrimitiveValue>(length));
    };

    rect->SetTop(get_length(rect_value->Top()));
    rect->SetRight(get_length(rect_value->Right()));
    rect->SetBottom(get_length(rect_value->Bottom()));
    rect->SetLeft(get_length(rect_value->Left()));

    InitializeBorderRadius(rect.get(), state, *rect_value);
    basic_shape = std::move(rect);
  } else if (const auto* xywh_value =
                 DynamicTo<cssvalue::CSSBasicShapeXYWHValue>(
                     basic_shape_value)) {
    scoped_refptr<BasicShapeXYWH> rect = BasicShapeXYWH::Create();

    rect->SetX(ConvertToLength(state, xywh_value->X()));
    rect->SetY(ConvertToLength(state, xywh_value->Y()));
    rect->SetWidth(ConvertToLength(state, xywh_value->Width()));
    rect->SetHeight(ConvertToLength(state, xywh_value->Height()));

    InitializeBorderRadius(rect.get(), state, *xywh_value);
    basic_shape = std::move(rect);
  } else if (const auto* ray_value =
                 DynamicTo<cssvalue::CSSRayValue>(basic_shape_value)) {
    float angle = ray_value->Angle().ComputeDegrees();
    StyleRay::RaySize size = KeywordToRaySize(ray_value->Size().GetValueID());
    bool contain = !!ray_value->Contain();
    basic_shape =
        StyleRay::Create(angle, size, contain,
                         ConvertToCenterCoordinate(state, ray_value->CenterX()),
                         ConvertToCenterCoordinate(state, ray_value->CenterY()),
                         ray_value->CenterX());
  } else if (const auto* path_value =
                 DynamicTo<cssvalue::CSSPathValue>(basic_shape_value)) {
    basic_shape = path_value->GetStylePath();
  } else {
    NOTREACHED();
  }

  return basic_shape;
}

}  // namespace blink
