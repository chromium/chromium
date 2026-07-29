// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"

#include <memory>
#include <optional>

#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/shape_property_functions.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class BasicShapeNonInterpolableValue : public NonInterpolableValue {
 public:
  explicit BasicShapeNonInterpolableValue(BasicShape::ShapeType type,
                                          ShapeReferenceBox box)
      : type_(type), wind_rule_(RULE_NONZERO), size_(0), box_(box) {
    DCHECK_NE(type, BasicShape::kBasicShapePolygonType);
  }
  BasicShapeNonInterpolableValue(WindRule wind_rule,
                                 wtf_size_t size,
                                 ShapeReferenceBox box)
      : type_(BasicShape::kBasicShapePolygonType),
        wind_rule_(wind_rule),
        size_(size),
        box_(box) {}
  BasicShapeNonInterpolableValue(const BasicShapeNonInterpolableValue&) =
      delete;

  BasicShape::ShapeType GetShapeType() const { return type_; }

  WindRule GetWindRule() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return wind_rule_;
  }
  wtf_size_t size() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return size_;
  }

  ShapeReferenceBox GetBox() const { return box_; }

  bool IsCompatibleWith(const BasicShapeNonInterpolableValue& other) const {
    if (GetShapeType() != other.GetShapeType() || GetBox() != other.GetBox()) {
      return false;
    }
    switch (GetShapeType()) {
      case BasicShape::kBasicShapeCircleType:
      case BasicShape::kBasicShapeEllipseType:
      case BasicShape::kBasicShapeInsetType:
        return true;
      case BasicShape::kBasicShapePolygonType:
        return GetWindRule() == other.GetWindRule() && size() == other.size();
      default:
        NOTREACHED();
    }
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 protected:
  const BasicShape::ShapeType type_;
  const WindRule wind_rule_;
  const wtf_size_t size_;
  const ShapeReferenceBox box_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(BasicShapeNonInterpolableValue);
template <>
struct DowncastTraits<BasicShapeNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == BasicShapeNonInterpolableValue::static_type_;
  }
};

namespace {

InterpolableValue* Unwrap(InterpolationValue&& value) {
  DCHECK(value.interpolable_value);
  return std::move(value.interpolable_value);
}

InterpolableValue* ConvertCSSCoordinate(const CSSValue* coordinate,
                                        const CSSProperty& property) {
  if (coordinate) {
    return Unwrap(
        CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
            *coordinate));
  }
  return InterpolableLength::MaybeConvertLength(
      Length::Percent(50), property, 1, /*interpolate_size=*/std::nullopt);
}

InterpolableValue* ConvertCoordinate(const Length& coordinate,
                                     const CSSProperty& property,
                                     double zoom) {
  return InterpolableLength::MaybeConvertLength(
      coordinate, property, zoom, /*interpolate_size=*/std::nullopt);
}

InterpolableValue* CreateNeutralInterpolableCoordinate() {
  return InterpolableLength::CreateNeutral();
}

Length CreateCoordinate(const InterpolableValue& interpolable_value,
                        const CSSToLengthConversionData& conversion_data) {
  return To<InterpolableLength>(interpolable_value)
      .CreateLength(conversion_data, Length::ValueRange::kAll);
}

InterpolableValue* ConvertCSSRadius(const CSSValue* radius) {
  if (!radius || radius->IsIdentifierValue()) {
    return nullptr;
  }
  return InterpolableLength::MaybeConvertCSSValue(*radius);
}

InterpolableValue* ConvertRadius(const BasicShapeRadius& radius,
                                 const CSSProperty& property,
                                 double zoom) {
  if (radius.GetType() != BasicShapeRadius::kValue) {
    return nullptr;
  }
  return InterpolableLength::MaybeConvertLength(
      radius.Value(), property, zoom,
      /*interpolate_size=*/std::nullopt);
}

InterpolableValue* CreateNeutralInterpolableRadius() {
  return InterpolableLength::CreateNeutral();
}

BasicShapeRadius CreateRadius(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  return BasicShapeRadius(
      To<InterpolableLength>(interpolable_value)
          .CreateLength(conversion_data, Length::ValueRange::kNonNegative));
}

InterpolableLength* ConvertCSSLength(const CSSValue& length) {
  return InterpolableLength::MaybeConvertCSSValue(length);
}

InterpolableLength* ConvertCSSLength(const CSSValue* length) {
  if (!length) {
    return InterpolableLength::CreateNeutral();
  }
  return ConvertCSSLength(*length);
}

InterpolableLength* ConvertCSSLengthOrAuto(const CSSValue& length,
                                           double auto_percent) {
  auto* identifier = DynamicTo<CSSIdentifierValue>(length);
  if (identifier && identifier->GetValueID() == CSSValueID::kAuto) {
    return InterpolableLength::CreatePercent(auto_percent);
  }
  return InterpolableLength::MaybeConvertCSSValue(length);
}

const CSSMathExpressionNode* AsExpressionNode(const CSSPrimitiveValue& value) {
  if (const auto* numeric_literal = DynamicTo<CSSNumericLiteralValue>(value)) {
    return CSSMathExpressionNumericLiteral::Create(numeric_literal);
  }
  return To<CSSMathFunctionValue>(value).ExpressionNode();
}

// Generate the expression: calc(minuend - subtrahend).
const CSSMathExpressionNode* SubtractCSSLength(
    const CSSMathExpressionNode& minuend,
    const CSSPrimitiveValue& subtrahend) {
  return CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
      &minuend, AsExpressionNode(subtrahend), CSSMathOperator::kSubtract);
}

// Produce a InterpolableLength from a CSSMathExpressionNode expression tree.
InterpolableLength* FinalizeExpression(
    const CSSMathExpressionNode& difference) {
  CSSLengthArray length_array;
  if (difference.AccumulateLengthArray(length_array, 1)) {
    return MakeGarbageCollected<InterpolableLength>(std::move(length_array));
  }
  return MakeGarbageCollected<InterpolableLength>(difference);
}

// Generate the expression: calc(100% - a - b).
InterpolableLength* ConvertCSSLengthsSubtractedFrom100Percent(
    const CSSPrimitiveValue& a,
    const CSSPrimitiveValue& b) {
  const auto* percent_100 = CSSMathExpressionNumericLiteral::Create(
      100, CSSPrimitiveValue::UnitType::kPercentage);
  return FinalizeExpression(
      *SubtractCSSLength(*SubtractCSSLength(*percent_100, a), b));
}

// Generate the expression: calc(100% - a).
InterpolableLength* ConvertCSSLengthSubtractedFrom100Percent(
    const CSSPrimitiveValue& a) {
  const auto* percent_100 = CSSMathExpressionNumericLiteral::Create(
      100, CSSPrimitiveValue::UnitType::kPercentage);
  return FinalizeExpression(*SubtractCSSLength(*percent_100, a));
}

InterpolableLength* ConvertCSSLengthOrAutoSubtractedFrom100Percent(
    const CSSValue& length,
    double auto_percent) {
  auto* identifier = DynamicTo<CSSIdentifierValue>(length);
  if (identifier && identifier->GetValueID() == CSSValueID::kAuto) {
    return InterpolableLength::CreatePercent(auto_percent);
  }
  return ConvertCSSLengthSubtractedFrom100Percent(
      To<CSSPrimitiveValue>(length));
}

InterpolableValue* ConvertLength(const Length& length,
                                 const CSSProperty& property,
                                 double zoom) {
  return InterpolableLength::MaybeConvertLength(
      length, property, zoom,
      /*interpolate_size=*/std::nullopt);
}

InterpolableValue* ConvertCSSBorderRadiusWidth(const CSSValuePair* pair) {
  return ConvertCSSLength(pair ? &pair->First() : nullptr);
}

InterpolableValue* ConvertCSSBorderRadiusHeight(const CSSValuePair* pair) {
  return ConvertCSSLength(pair ? &pair->Second() : nullptr);
}

LengthSize CreateBorderRadius(
    const InterpolableValue& width,
    const InterpolableValue& height,
    const CSSToLengthConversionData& conversion_data) {
  return LengthSize(To<InterpolableLength>(width).CreateLength(
                        conversion_data, Length::ValueRange::kNonNegative),
                    To<InterpolableLength>(height).CreateLength(
                        conversion_data, Length::ValueRange::kNonNegative));
}

namespace circle_functions {

enum CircleComponentIndex : unsigned {
  kCircleCenterXIndex,
  kCircleCenterYIndex,
  kCircleRadiusIndex,
  kCircleHasExplicitCenterIndex,
  kCircleComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeCircleValue& circle,
    const CSSProperty& property,
    ShapeReferenceBox box) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex,
            ConvertCSSCoordinate(circle.CenterX(), property));
  list->Set(kCircleCenterYIndex,
            ConvertCSSCoordinate(circle.CenterY(), property));
  list->Set(kCircleHasExplicitCenterIndex,
            MakeGarbageCollected<InterpolableNumber>(!!circle.CenterX()));

  InterpolableValue* radius = nullptr;
  if (!(radius = ConvertCSSRadius(circle.Radius()))) {
    return nullptr;
  }
  list->Set(kCircleRadiusIndex, radius);

  return InterpolationValue(
      std::move(list), MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                           BasicShape::kBasicShapeCircleType, box));
}

InterpolationValue ConvertBasicShape(const BasicShapeCircle& circle,
                                     const CSSProperty& property,
                                     ShapeReferenceBox box,
                                     double zoom) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex,
            ConvertCoordinate(circle.Center().X(), property, zoom));
  list->Set(kCircleCenterYIndex,
            ConvertCoordinate(circle.Center().Y(), property, zoom));
  list->Set(
      kCircleHasExplicitCenterIndex,
      MakeGarbageCollected<InterpolableNumber>(circle.HasExplicitCenter()));

  InterpolableValue* radius = nullptr;
  if (!(radius = ConvertRadius(circle.Radius(), property, zoom))) {
    return nullptr;
  }
  list->Set(kCircleRadiusIndex, radius);

  return InterpolationValue(
      std::move(list), MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                           BasicShape::kBasicShapeCircleType, box));
}

InterpolableValue* CreateNeutralValue() {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleRadiusIndex, CreateNeutralInterpolableRadius());
  list->Set(kCircleHasExplicitCenterIndex,
            MakeGarbageCollected<InterpolableNumber>(0));
  return list;
}

BasicShape* CreateBasicShape(const InterpolableValue& interpolable_value,
                             const CSSToLengthConversionData& conversion_data) {
  BasicShapeCircle* circle = MakeGarbageCollected<BasicShapeCircle>();
  const auto& list = To<InterpolableList>(interpolable_value);
  circle->SetCenter(LengthPoint(
      CreateCoordinate(*list.Get(kCircleCenterXIndex), conversion_data),
      CreateCoordinate(*list.Get(kCircleCenterYIndex), conversion_data)));
  circle->SetRadius(
      CreateRadius(*list.Get(kCircleRadiusIndex), conversion_data));
  circle->SetHasExplicitCenter(
      To<InterpolableNumber>(list.Get(kCircleHasExplicitCenterIndex))
          ->Value(conversion_data));
  return circle;
}

}  // namespace circle_functions

namespace ellipse_functions {

enum EllipseComponentIndex : unsigned {
  kEllipseCenterXIndex,
  kEllipseCenterYIndex,
  kEllipseRadiusXIndex,
  kEllipseRadiusYIndex,
  kEllipseHasExplicitCenter,
  kEllipseComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeEllipseValue& ellipse,
    const CSSProperty& property,
    ShapeReferenceBox box) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex,
            ConvertCSSCoordinate(ellipse.CenterX(), property));
  list->Set(kEllipseCenterYIndex,
            ConvertCSSCoordinate(ellipse.CenterY(), property));
  list->Set(kEllipseHasExplicitCenter,
            MakeGarbageCollected<InterpolableNumber>(!!ellipse.CenterX()));

  InterpolableValue* radius = nullptr;
  if (!(radius = ConvertCSSRadius(ellipse.RadiusX()))) {
    return nullptr;
  }
  list->Set(kEllipseRadiusXIndex, radius);
  if (!(radius = ConvertCSSRadius(ellipse.RadiusY()))) {
    return nullptr;
  }
  list->Set(kEllipseRadiusYIndex, radius);

  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                BasicShape::kBasicShapeEllipseType, box));
}

InterpolationValue ConvertBasicShape(const BasicShapeEllipse& ellipse,
                                     const CSSProperty& property,
                                     ShapeReferenceBox box,
                                     double zoom) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex,
            ConvertCoordinate(ellipse.Center().X(), property, zoom));
  list->Set(kEllipseCenterYIndex,
            ConvertCoordinate(ellipse.Center().Y(), property, zoom));
  list->Set(kEllipseHasExplicitCenter, MakeGarbageCollected<InterpolableNumber>(
                                           ellipse.HasExplicitCenter()));

  InterpolableValue* radius = nullptr;
  if (!(radius = ConvertRadius(ellipse.RadiusX(), property, zoom))) {
    return nullptr;
  }
  list->Set(kEllipseRadiusXIndex, radius);
  if (!(radius = ConvertRadius(ellipse.RadiusY(), property, zoom))) {
    return nullptr;
  }
  list->Set(kEllipseRadiusYIndex, radius);

  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                BasicShape::kBasicShapeEllipseType, box));
}

InterpolableValue* CreateNeutralValue() {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseRadiusXIndex, CreateNeutralInterpolableRadius());
  list->Set(kEllipseRadiusYIndex, CreateNeutralInterpolableRadius());
  list->Set(kEllipseHasExplicitCenter,
            MakeGarbageCollected<InterpolableNumber>(0));
  return list;
}

BasicShape* CreateBasicShape(const InterpolableValue& interpolable_value,
                             const CSSToLengthConversionData& conversion_data) {
  BasicShapeEllipse* ellipse = MakeGarbageCollected<BasicShapeEllipse>();
  const auto& list = To<InterpolableList>(interpolable_value);
  ellipse->SetCenter(LengthPoint(
      CreateCoordinate(*list.Get(kEllipseCenterXIndex), conversion_data),
      CreateCoordinate(*list.Get(kEllipseCenterYIndex), conversion_data)));
  ellipse->SetRadiusX(
      CreateRadius(*list.Get(kEllipseRadiusXIndex), conversion_data));
  ellipse->SetRadiusY(
      CreateRadius(*list.Get(kEllipseRadiusYIndex), conversion_data));
  ellipse->SetHasExplicitCenter(
      To<InterpolableNumber>(list.Get(kEllipseHasExplicitCenter))
          ->Value(conversion_data));
  return ellipse;
}

}  // namespace ellipse_functions

namespace inset_functions {

enum InsetComponentIndex : unsigned {
  kInsetTopIndex,
  kInsetRightIndex,
  kInsetBottomIndex,
  kInsetLeftIndex,
  kInsetBorderTopLeftWidthIndex,
  kInsetBorderTopLeftHeightIndex,
  kInsetBorderTopRightWidthIndex,
  kInsetBorderTopRightHeightIndex,
  kInsetBorderBottomRightWidthIndex,
  kInsetBorderBottomRightHeightIndex,
  kInsetBorderBottomLeftWidthIndex,
  kInsetBorderBottomLeftHeightIndex,
  kInsetComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeInsetValue& inset,
    ShapeReferenceBox box) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex, ConvertCSSLength(inset.Top()));
  list->Set(kInsetRightIndex, ConvertCSSLength(inset.Right()));
  list->Set(kInsetBottomIndex, ConvertCSSLength(inset.Bottom()));
  list->Set(kInsetLeftIndex, ConvertCSSLength(inset.Left()));

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.TopLeftRadius()));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.TopLeftRadius()));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.TopRightRadius()));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.TopRightRadius()));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.BottomRightRadius()));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.BottomRightRadius()));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(inset.BottomLeftRadius()));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(inset.BottomLeftRadius()));
  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                BasicShape::kBasicShapeInsetType, box));
}

void FillCanonicalRect(InterpolableList* list,
                       const cssvalue::CSSBasicShapeRectValue& rect) {
  // rect(t r b l) => inset(t calc(100% - r) calc(100% - b) l).
  list->Set(kInsetTopIndex, ConvertCSSLengthOrAuto(rect.Top(), 0));
  list->Set(kInsetRightIndex,
            ConvertCSSLengthOrAutoSubtractedFrom100Percent(rect.Right(), 0));
  list->Set(kInsetBottomIndex,
            ConvertCSSLengthOrAutoSubtractedFrom100Percent(rect.Bottom(), 0));
  list->Set(kInsetLeftIndex, ConvertCSSLengthOrAuto(rect.Left(), 0));
}

void FillCanonicalRect(InterpolableList* list,
                       const cssvalue::CSSBasicShapeXYWHValue& xywh) {
  // xywh(x y w h) => inset(y calc(100% - (x + w)) calc(100% - (y + h)) x).
  const CSSPrimitiveValue& x = xywh.X();
  const CSSPrimitiveValue& y = xywh.Y();
  const CSSPrimitiveValue& w = xywh.Width();
  const CSSPrimitiveValue& h = xywh.Height();
  list->Set(kInsetTopIndex, ConvertCSSLength(y));
  // calc(100% - (x + w)) = calc(100% - x - w).
  list->Set(kInsetRightIndex, ConvertCSSLengthsSubtractedFrom100Percent(x, w));
  // calc(100% - (y + h)) = calc(100% - y - h).
  list->Set(kInsetBottomIndex, ConvertCSSLengthsSubtractedFrom100Percent(y, h));
  list->Set(kInsetLeftIndex, ConvertCSSLength(x));
}

template <typename BasicShapeCSSValueClass>
InterpolationValue ConvertCSSValueToInset(const BasicShapeCSSValueClass& rect,
                                          ShapeReferenceBox box) {
  // Spec: All <basic-shape-rect> functions compute to the equivalent
  // inset() function.

  // NOTE: Given `xywh(x y w h)`, the equivalent function is `inset(y
  // calc(100% - x - w) calc(100% - y - h) x)`.  See:
  // https://drafts.csswg.org/css-shapes/#basic-shape-computed-values and
  // https://github.com/w3c/csswg-drafts/issues/9053
  auto* list =
      MakeGarbageCollected<InterpolableList>(kInsetComponentIndexCount);
  FillCanonicalRect(list, rect);

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(rect.TopLeftRadius()));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(rect.TopLeftRadius()));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertCSSBorderRadiusWidth(rect.TopRightRadius()));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertCSSBorderRadiusHeight(rect.TopRightRadius()));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertCSSBorderRadiusWidth(rect.BottomRightRadius()));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertCSSBorderRadiusHeight(rect.BottomRightRadius()));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(rect.BottomLeftRadius()));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(rect.BottomLeftRadius()));
  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                BasicShape::kBasicShapeInsetType, box));
}

InterpolationValue ConvertBasicShape(const BasicShapeInset& inset,
                                     const CSSProperty& property,
                                     ShapeReferenceBox box,
                                     double zoom) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex, ConvertLength(inset.Top(), property, zoom));
  list->Set(kInsetRightIndex, ConvertLength(inset.Right(), property, zoom));
  list->Set(kInsetBottomIndex, ConvertLength(inset.Bottom(), property, zoom));
  list->Set(kInsetLeftIndex, ConvertLength(inset.Left(), property, zoom));

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertLength(inset.TopLeftRadius().Width(), property, zoom));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertLength(inset.TopLeftRadius().Height(), property, zoom));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertLength(inset.TopRightRadius().Width(), property, zoom));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertLength(inset.TopRightRadius().Height(), property, zoom));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertLength(inset.BottomRightRadius().Width(), property, zoom));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertLength(inset.BottomRightRadius().Height(), property, zoom));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertLength(inset.BottomLeftRadius().Width(), property, zoom));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertLength(inset.BottomLeftRadius().Height(), property, zoom));
  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                BasicShape::kBasicShapeInsetType, box));
}

InterpolableValue* CreateNeutralValue() {
  auto* list =
      MakeGarbageCollected<InterpolableList>(kInsetComponentIndexCount);
  list->Set(kInsetTopIndex, InterpolableLength::CreateNeutral());
  list->Set(kInsetRightIndex, InterpolableLength::CreateNeutral());
  list->Set(kInsetBottomIndex, InterpolableLength::CreateNeutral());
  list->Set(kInsetLeftIndex, InterpolableLength::CreateNeutral());

  list->Set(kInsetBorderTopLeftWidthIndex, InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderTopLeftHeightIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderTopRightWidthIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderTopRightHeightIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderBottomRightWidthIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderBottomRightHeightIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderBottomLeftWidthIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kInsetBorderBottomLeftHeightIndex,
            InterpolableLength::CreateNeutral());
  return list;
}

BasicShape* CreateBasicShape(const InterpolableValue& interpolable_value,
                             const CSSToLengthConversionData& conversion_data) {
  const auto& list = To<InterpolableList>(interpolable_value);

  BasicShapeInset* inset = MakeGarbageCollected<BasicShapeInset>();
  inset->SetTop(To<InterpolableLength>(*list.Get(kInsetTopIndex))
                    .CreateLength(conversion_data, Length::ValueRange::kAll));
  inset->SetRight(To<InterpolableLength>(*list.Get(kInsetRightIndex))
                      .CreateLength(conversion_data, Length::ValueRange::kAll));
  inset->SetBottom(
      To<InterpolableLength>(*list.Get(kInsetBottomIndex))
          .CreateLength(conversion_data, Length::ValueRange::kAll));
  inset->SetLeft(To<InterpolableLength>(*list.Get(kInsetLeftIndex))
                     .CreateLength(conversion_data, Length::ValueRange::kAll));

  inset->SetTopLeftRadius(CreateBorderRadius(
      *list.Get(kInsetBorderTopLeftWidthIndex),
      *list.Get(kInsetBorderTopLeftHeightIndex), conversion_data));
  inset->SetTopRightRadius(CreateBorderRadius(
      *list.Get(kInsetBorderTopRightWidthIndex),
      *list.Get(kInsetBorderTopRightHeightIndex), conversion_data));
  inset->SetBottomRightRadius(CreateBorderRadius(
      *list.Get(kInsetBorderBottomRightWidthIndex),
      *list.Get(kInsetBorderBottomRightHeightIndex), conversion_data));
  inset->SetBottomLeftRadius(CreateBorderRadius(
      *list.Get(kInsetBorderBottomLeftWidthIndex),
      *list.Get(kInsetBorderBottomLeftHeightIndex), conversion_data));
  return inset;
}

}  // namespace inset_functions

namespace polygon_functions {

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapePolygonValue& polygon,
    ShapeReferenceBox box) {
  wtf_size_t size = polygon.Values().size();
  auto* list = MakeGarbageCollected<InterpolableList>(size + 1);
  for (wtf_size_t i = 0; i < size; i++) {
    list->Set(i, ConvertCSSLength(polygon.Values()[i].Get()));
  }
  list->Set(size, ConvertCSSLength(polygon.RoundingRadius()));
  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                polygon.GetWindRule(), size, box));
}

InterpolationValue ConvertBasicShape(const BasicShapePolygon& polygon,
                                     const CSSProperty& property,
                                     ShapeReferenceBox box,
                                     double zoom) {
  wtf_size_t size = polygon.Values().size();
  auto* list = MakeGarbageCollected<InterpolableList>(size + 1);
  for (wtf_size_t i = 0; i < size; i++) {
    list->Set(i, ConvertLength(polygon.Values()[i], property, zoom));
  }
  list->Set(size, ConvertLength(polygon.RoundingRadius(), property, zoom));
  return InterpolationValue(
      list, MakeGarbageCollected<BasicShapeNonInterpolableValue>(
                polygon.GetWindRule(), size, box));
}

InterpolableValue* CreateNeutralValue(
    const BasicShapeNonInterpolableValue& non_interpolable_value) {
  auto* list =
      MakeGarbageCollected<InterpolableList>(non_interpolable_value.size() + 1);
  for (wtf_size_t i = 0; i < non_interpolable_value.size(); i++) {
    list->Set(i, InterpolableLength::CreateNeutral());
  }
  list->Set(non_interpolable_value.size(), InterpolableLength::CreateNeutral());
  return list;
}

BasicShape* CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const BasicShapeNonInterpolableValue& non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  BasicShapePolygon* polygon = MakeGarbageCollected<BasicShapePolygon>();
  polygon->SetWindRule(non_interpolable_value.GetWindRule());
  const auto& list = To<InterpolableList>(interpolable_value);
  wtf_size_t size = non_interpolable_value.size();
  DCHECK_EQ(list.length(), size + 1);
  DCHECK_EQ(size % 2, 0U);
  for (wtf_size_t i = 0; i < size; i += 2) {
    polygon->AppendPoint(
        To<InterpolableLength>(*list.Get(i))
            .CreateLength(conversion_data, Length::ValueRange::kAll),
        To<InterpolableLength>(*list.Get(i + 1))
            .CreateLength(conversion_data, Length::ValueRange::kAll));
  }
  polygon->SetRoundingRadius(
      To<InterpolableLength>(*list.Get(size))
          .CreateLength(conversion_data, Length::ValueRange::kNonNegative));
  return polygon;
}

}  // namespace polygon_functions

}  // namespace

InterpolationValue basic_shape_interpolation_functions::MaybeConvertCSSValue(
    const BasicShapeCssInfo& info,
    const CSSProperty& property) {
  const CSSValue& value = *info.shape;
  if (auto* circle_value =
          DynamicTo<cssvalue::CSSBasicShapeCircleValue>(value)) {
    return circle_functions::ConvertCSSValue(*circle_value, property, info.box);
  }
  if (auto* ellipse_value =
          DynamicTo<cssvalue::CSSBasicShapeEllipseValue>(value)) {
    return ellipse_functions::ConvertCSSValue(*ellipse_value, property,
                                              info.box);
  }
  if (auto* inset_value = DynamicTo<cssvalue::CSSBasicShapeInsetValue>(value)) {
    return inset_functions::ConvertCSSValue(*inset_value, info.box);
  }
  if (auto* rect_value = DynamicTo<cssvalue::CSSBasicShapeRectValue>(value)) {
    return inset_functions::ConvertCSSValueToInset(*rect_value, info.box);
  }
  if (auto* xywh_value = DynamicTo<cssvalue::CSSBasicShapeXYWHValue>(value)) {
    return inset_functions::ConvertCSSValueToInset(*xywh_value, info.box);
  }
  if (auto* polygon_value =
          DynamicTo<cssvalue::CSSBasicShapePolygonValue>(value)) {
    return polygon_functions::ConvertCSSValue(*polygon_value, info.box);
  }
  return nullptr;
}

InterpolationValue basic_shape_interpolation_functions::MaybeConvertBasicShape(
    const BasicShapeInfo& info,
    const CSSProperty& property,
    double zoom) {
  if (!info.shape) {
    return nullptr;
  }
  switch (info.shape->GetType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::ConvertBasicShape(
          To<BasicShapeCircle>(*info.shape), property, info.box, zoom);
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::ConvertBasicShape(
          To<BasicShapeEllipse>(*info.shape), property, info.box, zoom);
    case BasicShape::kBasicShapeInsetType:
      return inset_functions::ConvertBasicShape(
          To<BasicShapeInset>(*info.shape), property, info.box, zoom);
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::ConvertBasicShape(
          To<BasicShapePolygon>(*info.shape), property, info.box, zoom);
    // Handled by PathInterpolationFunction.
    case BasicShape::kStylePathType:
    case BasicShape::kStyleShapeType:
      return nullptr;
    default:
      NOTREACHED();
  }
}

InterpolableValue* basic_shape_interpolation_functions::CreateNeutralValue(
    const NonInterpolableValue& untyped_non_interpolable_value) {
  const auto& non_interpolable_value =
      To<BasicShapeNonInterpolableValue>(untyped_non_interpolable_value);
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeInsetType:
      return inset_functions::CreateNeutralValue();
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::CreateNeutralValue(non_interpolable_value);
    default:
      NOTREACHED();
  }
}

bool basic_shape_interpolation_functions::ShapesAreCompatible(
    const NonInterpolableValue& a,
    const NonInterpolableValue& b) {
  return To<BasicShapeNonInterpolableValue>(a).IsCompatibleWith(
      To<BasicShapeNonInterpolableValue>(b));
}

BasicShapeInfo basic_shape_interpolation_functions::CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  const auto& non_interpolable_value =
      To<BasicShapeNonInterpolableValue>(untyped_non_interpolable_value);
  BasicShape* shape;
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      shape = circle_functions::CreateBasicShape(interpolable_value,
                                                 conversion_data);
      break;
    case BasicShape::kBasicShapeEllipseType:
      shape = ellipse_functions::CreateBasicShape(interpolable_value,
                                                  conversion_data);
      break;
    case BasicShape::kBasicShapeInsetType:
      shape = inset_functions::CreateBasicShape(interpolable_value,
                                                conversion_data);
      break;
    case BasicShape::kBasicShapePolygonType:
      shape = polygon_functions::CreateBasicShape(
          interpolable_value, non_interpolable_value, conversion_data);
      break;
    default:
      NOTREACHED();
  }
  return {shape, non_interpolable_value.GetBox()};
}

}  // namespace blink
