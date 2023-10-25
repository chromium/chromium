// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"

namespace blink {

class BasicShapeNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<const NonInterpolableValue> Create(
      BasicShape::ShapeType type) {
    return base::AdoptRef(new BasicShapeNonInterpolableValue(type));
  }
  static scoped_refptr<const NonInterpolableValue> CreatePolygon(
      WindRule wind_rule,
      wtf_size_t size) {
    return base::AdoptRef(new BasicShapeNonInterpolableValue(wind_rule, size));
  }

  BasicShape::ShapeType GetShapeType() const { return type_; }

  WindRule GetWindRule() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return wind_rule_;
  }
  wtf_size_t size() const {
    DCHECK_EQ(GetShapeType(), BasicShape::kBasicShapePolygonType);
    return size_;
  }

  bool IsCompatibleWith(const BasicShapeNonInterpolableValue& other) const {
    if (GetShapeType() != other.GetShapeType())
      return false;
    switch (GetShapeType()) {
      case BasicShape::kBasicShapeCircleType:
      case BasicShape::kBasicShapeEllipseType:
      case BasicShape::kBasicShapeInsetType:
      case BasicShape::kBasicShapeRectType:
      case BasicShape::kBasicShapeXYWHType:
        return true;
      case BasicShape::kBasicShapePolygonType:
        return GetWindRule() == other.GetWindRule() && size() == other.size();
      default:
        NOTREACHED();
        return false;
    }
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  BasicShapeNonInterpolableValue(BasicShape::ShapeType type)
      : type_(type), wind_rule_(RULE_NONZERO), size_(0) {
    DCHECK_NE(type, BasicShape::kBasicShapePolygonType);
  }
  BasicShapeNonInterpolableValue(WindRule wind_rule, wtf_size_t size)
      : type_(BasicShape::kBasicShapePolygonType),
        wind_rule_(wind_rule),
        size_(size) {}

  const BasicShape::ShapeType type_;
  const WindRule wind_rule_;
  const wtf_size_t size_;
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

std::unique_ptr<InterpolableValue> Unwrap(InterpolationValue&& value) {
  DCHECK(value.interpolable_value);
  return std::move(value.interpolable_value);
}

std::unique_ptr<InterpolableValue> ConvertCSSCoordinate(
    const CSSValue* coordinate) {
  if (coordinate)
    return Unwrap(
        CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
            *coordinate));
  return InterpolableLength::MaybeConvertLength(Length::Percent(50), 1);
}

std::unique_ptr<InterpolableValue> ConvertCoordinate(
    const BasicShapeCenterCoordinate& coordinate,
    double zoom) {
  return InterpolableLength::MaybeConvertLength(coordinate.ComputedLength(),
                                                zoom);
}

std::unique_ptr<InterpolableValue> CreateNeutralInterpolableCoordinate() {
  return InterpolableLength::CreateNeutral();
}

BasicShapeCenterCoordinate CreateCoordinate(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  return BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft,
      To<InterpolableLength>(interpolable_value)
          .CreateLength(conversion_data, Length::ValueRange::kAll));
}

std::unique_ptr<InterpolableValue> ConvertCSSRadius(const CSSValue* radius) {
  if (!radius || radius->IsIdentifierValue())
    return nullptr;
  return InterpolableLength::MaybeConvertCSSValue(*radius);
}

std::unique_ptr<InterpolableValue> ConvertRadius(const BasicShapeRadius& radius,
                                                 double zoom) {
  if (radius.GetType() != BasicShapeRadius::kValue)
    return nullptr;
  return InterpolableLength::MaybeConvertLength(radius.Value(), zoom);
}

std::unique_ptr<InterpolableValue> CreateNeutralInterpolableRadius() {
  return InterpolableLength::CreateNeutral();
}

BasicShapeRadius CreateRadius(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  return BasicShapeRadius(
      To<InterpolableLength>(interpolable_value)
          .CreateLength(conversion_data, Length::ValueRange::kNonNegative));
}

std::unique_ptr<InterpolableValue> ConvertCSSLength(const CSSValue* length) {
  if (!length)
    return InterpolableLength::CreateNeutral();
  return InterpolableLength::MaybeConvertCSSValue(*length);
}

std::unique_ptr<InterpolableValue> ConvertCSSLengthOrAuto(
    const CSSValue* length,
    double auto_percent) {
  if (!length) {
    return InterpolableLength::CreateNeutral();
  }
  auto* identifier = DynamicTo<CSSIdentifierValue>(length);
  if (identifier && identifier->GetValueID() == CSSValueID::kAuto) {
    return InterpolableLength::CreatePercent(auto_percent);
  }
  return InterpolableLength::MaybeConvertCSSValue(*length);
}

std::unique_ptr<InterpolableValue> ConvertLength(const Length& length,
                                                 double zoom) {
  return InterpolableLength::MaybeConvertLength(length, zoom);
}

std::unique_ptr<InterpolableValue> ConvertLengthOrAuto(const Length& length,
                                                       double zoom,
                                                       double auto_percent) {
  if (length.IsAuto()) {
    return InterpolableLength::CreatePercent(auto_percent);
  }
  return ConvertLength(length, zoom);
}

std::unique_ptr<InterpolableValue> ConvertCSSBorderRadiusWidth(
    const CSSValuePair* pair) {
  return ConvertCSSLength(pair ? &pair->First() : nullptr);
}

std::unique_ptr<InterpolableValue> ConvertCSSBorderRadiusHeight(
    const CSSValuePair* pair) {
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
    const cssvalue::CSSBasicShapeCircleValue& circle) {
  auto list = std::make_unique<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, ConvertCSSCoordinate(circle.CenterX()));
  list->Set(kCircleCenterYIndex, ConvertCSSCoordinate(circle.CenterY()));
  list->Set(kCircleHasExplicitCenterIndex,
            std::make_unique<InterpolableNumber>(!!circle.CenterX()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertCSSRadius(circle.Radius())))
    return nullptr;
  list->Set(kCircleRadiusIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeCircleType));
}

InterpolationValue ConvertBasicShape(const BasicShapeCircle& circle,
                                     double zoom) {
  auto list = std::make_unique<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, ConvertCoordinate(circle.CenterX(), zoom));
  list->Set(kCircleCenterYIndex, ConvertCoordinate(circle.CenterY(), zoom));
  list->Set(kCircleHasExplicitCenterIndex,
            std::make_unique<InterpolableNumber>(circle.HasExplicitCenter()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertRadius(circle.Radius(), zoom)))
    return nullptr;
  list->Set(kCircleRadiusIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeCircleType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  auto list = std::make_unique<InterpolableList>(kCircleComponentIndexCount);
  list->Set(kCircleCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kCircleRadiusIndex, CreateNeutralInterpolableRadius());
  list->Set(kCircleHasExplicitCenterIndex,
            std::make_unique<InterpolableNumber>(0));
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeCircle> circle = BasicShapeCircle::Create();
  const auto& list = To<InterpolableList>(interpolable_value);
  circle->SetCenterX(
      CreateCoordinate(*list.Get(kCircleCenterXIndex), conversion_data));
  circle->SetCenterY(
      CreateCoordinate(*list.Get(kCircleCenterYIndex), conversion_data));
  circle->SetRadius(
      CreateRadius(*list.Get(kCircleRadiusIndex), conversion_data));
  circle->SetHasExplicitCenter(
      To<InterpolableNumber>(list.Get(kCircleHasExplicitCenterIndex))->Value());
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
    const cssvalue::CSSBasicShapeEllipseValue& ellipse) {
  auto list = std::make_unique<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, ConvertCSSCoordinate(ellipse.CenterX()));
  list->Set(kEllipseCenterYIndex, ConvertCSSCoordinate(ellipse.CenterY()));
  list->Set(kEllipseHasExplicitCenter,
            std::make_unique<InterpolableNumber>(!!ellipse.CenterX()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertCSSRadius(ellipse.RadiusX())))
    return nullptr;
  list->Set(kEllipseRadiusXIndex, std::move(radius));
  if (!(radius = ConvertCSSRadius(ellipse.RadiusY())))
    return nullptr;
  list->Set(kEllipseRadiusYIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeEllipseType));
}

InterpolationValue ConvertBasicShape(const BasicShapeEllipse& ellipse,
                                     double zoom) {
  auto list = std::make_unique<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, ConvertCoordinate(ellipse.CenterX(), zoom));
  list->Set(kEllipseCenterYIndex, ConvertCoordinate(ellipse.CenterY(), zoom));
  list->Set(kEllipseHasExplicitCenter,
            std::make_unique<InterpolableNumber>(ellipse.HasExplicitCenter()));

  std::unique_ptr<InterpolableValue> radius;
  if (!(radius = ConvertRadius(ellipse.RadiusX(), zoom)))
    return nullptr;
  list->Set(kEllipseRadiusXIndex, std::move(radius));
  if (!(radius = ConvertRadius(ellipse.RadiusY(), zoom)))
    return nullptr;
  list->Set(kEllipseRadiusYIndex, std::move(radius));

  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(
                                BasicShape::kBasicShapeEllipseType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  auto list = std::make_unique<InterpolableList>(kEllipseComponentIndexCount);
  list->Set(kEllipseCenterXIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseCenterYIndex, CreateNeutralInterpolableCoordinate());
  list->Set(kEllipseRadiusXIndex, CreateNeutralInterpolableRadius());
  list->Set(kEllipseRadiusYIndex, CreateNeutralInterpolableRadius());
  list->Set(kEllipseHasExplicitCenter, std::make_unique<InterpolableNumber>(0));
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeEllipse> ellipse = BasicShapeEllipse::Create();
  const auto& list = To<InterpolableList>(interpolable_value);
  ellipse->SetCenterX(
      CreateCoordinate(*list.Get(kEllipseCenterXIndex), conversion_data));
  ellipse->SetCenterY(
      CreateCoordinate(*list.Get(kEllipseCenterYIndex), conversion_data));
  ellipse->SetRadiusX(
      CreateRadius(*list.Get(kEllipseRadiusXIndex), conversion_data));
  ellipse->SetRadiusY(
      CreateRadius(*list.Get(kEllipseRadiusYIndex), conversion_data));
  ellipse->SetHasExplicitCenter(
      To<InterpolableNumber>(list.Get(kEllipseHasExplicitCenter))->Value());
  return ellipse;
}

}  // namespace ellipse_functions

namespace rect_common_functions {

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

template <typename BasicShapeCSSValueClass>
InterpolationValue ConvertCSSValue(const BasicShapeCSSValueClass& inset) {
  BasicShape::ShapeType type;
  if (inset.IsBasicShapeInsetValue()) {
    type = BasicShape::kBasicShapeInsetType;
  } else {
    DCHECK(inset.IsBasicShapeRectValue());
    type = BasicShape::kBasicShapeRectType;
  }

  auto list = std::make_unique<InterpolableList>(kInsetComponentIndexCount);
  // 'auto' can only appear in the rect() function, but passing for inset()
  // where it won't be used for simplicity.
  list->Set(kInsetTopIndex, ConvertCSSLengthOrAuto(inset.Top(), 0));
  list->Set(kInsetRightIndex, ConvertCSSLengthOrAuto(inset.Right(), 100));
  list->Set(kInsetBottomIndex, ConvertCSSLengthOrAuto(inset.Bottom(), 100));
  list->Set(kInsetLeftIndex, ConvertCSSLengthOrAuto(inset.Left(), 0));

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
  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::Create(type));
}

InterpolationValue ConvertBasicShape(const BasicShapeRectCommon& inset,
                                     double zoom) {
  auto list = std::make_unique<InterpolableList>(kInsetComponentIndexCount);
  // 'auto' can only appear in the rect() function, but passing for inset()
  // where it won't be used for simplicity.
  list->Set(kInsetTopIndex, ConvertLengthOrAuto(inset.Top(), zoom, 0));
  list->Set(kInsetRightIndex, ConvertLengthOrAuto(inset.Right(), zoom, 100));
  list->Set(kInsetBottomIndex, ConvertLengthOrAuto(inset.Bottom(), zoom, 100));
  list->Set(kInsetLeftIndex, ConvertLengthOrAuto(inset.Left(), zoom, 0));

  list->Set(kInsetBorderTopLeftWidthIndex,
            ConvertLength(inset.TopLeftRadius().Width(), zoom));
  list->Set(kInsetBorderTopLeftHeightIndex,
            ConvertLength(inset.TopLeftRadius().Height(), zoom));
  list->Set(kInsetBorderTopRightWidthIndex,
            ConvertLength(inset.TopRightRadius().Width(), zoom));
  list->Set(kInsetBorderTopRightHeightIndex,
            ConvertLength(inset.TopRightRadius().Height(), zoom));
  list->Set(kInsetBorderBottomRightWidthIndex,
            ConvertLength(inset.BottomRightRadius().Width(), zoom));
  list->Set(kInsetBorderBottomRightHeightIndex,
            ConvertLength(inset.BottomRightRadius().Height(), zoom));
  list->Set(kInsetBorderBottomLeftWidthIndex,
            ConvertLength(inset.BottomLeftRadius().Width(), zoom));
  list->Set(kInsetBorderBottomLeftHeightIndex,
            ConvertLength(inset.BottomLeftRadius().Height(), zoom));
  return InterpolationValue(
      std::move(list), BasicShapeNonInterpolableValue::Create(inset.GetType()));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  auto list = std::make_unique<InterpolableList>(kInsetComponentIndexCount);
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
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    BasicShape::ShapeType type,
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapeRectCommon> inset;
  if (type == BasicShape::kBasicShapeInsetType) {
    inset = BasicShapeInset::Create();
  } else {
    DCHECK_EQ(type, BasicShape::kBasicShapeRectType);
    inset = BasicShapeRect::Create();
  }

  const auto& list = To<InterpolableList>(interpolable_value);
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

}  // namespace rect_common_functions

namespace xywh_functions {

enum XywhComponentIndex : unsigned {
  kXywhXIndex,
  kXywhYIndex,
  kXywhWidthIndex,
  kXywhHeightIndex,
  kXywhBorderTopLeftWidthIndex,
  kXywhBorderTopLeftHeightIndex,
  kXywhBorderTopRightWidthIndex,
  kXywhBorderTopRightHeightIndex,
  kXywhBorderBottomRightWidthIndex,
  kXywhBorderBottomRightHeightIndex,
  kXywhBorderBottomLeftWidthIndex,
  kXywhBorderBottomLeftHeightIndex,
  kXywhComponentIndexCount,
};

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapeXYWHValue& value) {
  auto list = std::make_unique<InterpolableList>(kXywhComponentIndexCount);
  list->Set(kXywhXIndex, ConvertCSSLength(value.X()));
  list->Set(kXywhYIndex, ConvertCSSLength(value.Y()));
  list->Set(kXywhWidthIndex, ConvertCSSLength(value.Width()));
  list->Set(kXywhHeightIndex, ConvertCSSLength(value.Height()));

  list->Set(kXywhBorderTopLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(value.TopLeftRadius()));
  list->Set(kXywhBorderTopLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(value.TopLeftRadius()));
  list->Set(kXywhBorderTopRightWidthIndex,
            ConvertCSSBorderRadiusWidth(value.TopRightRadius()));
  list->Set(kXywhBorderTopRightHeightIndex,
            ConvertCSSBorderRadiusHeight(value.TopRightRadius()));
  list->Set(kXywhBorderBottomRightWidthIndex,
            ConvertCSSBorderRadiusWidth(value.BottomRightRadius()));
  list->Set(kXywhBorderBottomRightHeightIndex,
            ConvertCSSBorderRadiusHeight(value.BottomRightRadius()));
  list->Set(kXywhBorderBottomLeftWidthIndex,
            ConvertCSSBorderRadiusWidth(value.BottomLeftRadius()));
  list->Set(kXywhBorderBottomLeftHeightIndex,
            ConvertCSSBorderRadiusHeight(value.BottomLeftRadius()));
  return InterpolationValue(
      std::move(list),
      BasicShapeNonInterpolableValue::Create(BasicShape::kBasicShapeXYWHType));
}

InterpolationValue ConvertBasicShape(const BasicShapeXYWH& shape, double zoom) {
  auto list = std::make_unique<InterpolableList>(kXywhComponentIndexCount);
  list->Set(kXywhXIndex, ConvertLength(shape.X(), zoom));
  list->Set(kXywhYIndex, ConvertLength(shape.Y(), zoom));
  list->Set(kXywhWidthIndex, ConvertLength(shape.Width(), zoom));
  list->Set(kXywhHeightIndex, ConvertLength(shape.Height(), zoom));

  list->Set(kXywhBorderTopLeftWidthIndex,
            ConvertLength(shape.TopLeftRadius().Width(), zoom));
  list->Set(kXywhBorderTopLeftHeightIndex,
            ConvertLength(shape.TopLeftRadius().Height(), zoom));
  list->Set(kXywhBorderTopRightWidthIndex,
            ConvertLength(shape.TopRightRadius().Width(), zoom));
  list->Set(kXywhBorderTopRightHeightIndex,
            ConvertLength(shape.TopRightRadius().Height(), zoom));
  list->Set(kXywhBorderBottomRightWidthIndex,
            ConvertLength(shape.BottomRightRadius().Width(), zoom));
  list->Set(kXywhBorderBottomRightHeightIndex,
            ConvertLength(shape.BottomRightRadius().Height(), zoom));
  list->Set(kXywhBorderBottomLeftWidthIndex,
            ConvertLength(shape.BottomLeftRadius().Width(), zoom));
  list->Set(kXywhBorderBottomLeftHeightIndex,
            ConvertLength(shape.BottomLeftRadius().Height(), zoom));
  return InterpolationValue(
      std::move(list),
      BasicShapeNonInterpolableValue::Create(BasicShape::kBasicShapeXYWHType));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue() {
  auto list = std::make_unique<InterpolableList>(kXywhComponentIndexCount);
  list->Set(kXywhXIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhYIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhWidthIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhHeightIndex, InterpolableLength::CreateNeutral());

  list->Set(kXywhBorderTopLeftWidthIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderTopLeftHeightIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderTopRightWidthIndex, InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderTopRightHeightIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderBottomRightWidthIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderBottomRightHeightIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderBottomLeftWidthIndex,
            InterpolableLength::CreateNeutral());
  list->Set(kXywhBorderBottomLeftHeightIndex,
            InterpolableLength::CreateNeutral());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  auto shape = BasicShapeXYWH::Create();
  const auto& list = To<InterpolableList>(interpolable_value);
  shape->SetX(To<InterpolableLength>(*list.Get(kXywhXIndex))
                  .CreateLength(conversion_data, Length::ValueRange::kAll));
  shape->SetY(To<InterpolableLength>(*list.Get(kXywhYIndex))
                  .CreateLength(conversion_data, Length::ValueRange::kAll));
  shape->SetWidth(To<InterpolableLength>(*list.Get(kXywhWidthIndex))
                      .CreateLength(conversion_data, Length::ValueRange::kAll));
  shape->SetHeight(
      To<InterpolableLength>(*list.Get(kXywhHeightIndex))
          .CreateLength(conversion_data, Length::ValueRange::kAll));

  shape->SetTopLeftRadius(CreateBorderRadius(
      *list.Get(kXywhBorderTopLeftWidthIndex),
      *list.Get(kXywhBorderTopLeftHeightIndex), conversion_data));
  shape->SetTopRightRadius(CreateBorderRadius(
      *list.Get(kXywhBorderTopRightWidthIndex),
      *list.Get(kXywhBorderTopRightHeightIndex), conversion_data));
  shape->SetBottomRightRadius(CreateBorderRadius(
      *list.Get(kXywhBorderBottomRightWidthIndex),
      *list.Get(kXywhBorderBottomRightHeightIndex), conversion_data));
  shape->SetBottomLeftRadius(CreateBorderRadius(
      *list.Get(kXywhBorderBottomLeftWidthIndex),
      *list.Get(kXywhBorderBottomLeftHeightIndex), conversion_data));
  return shape;
}

}  // namespace xywh_functions

namespace polygon_functions {

InterpolationValue ConvertCSSValue(
    const cssvalue::CSSBasicShapePolygonValue& polygon) {
  wtf_size_t size = polygon.Values().size();
  auto list = std::make_unique<InterpolableList>(size);
  for (wtf_size_t i = 0; i < size; i++)
    list->Set(i, ConvertCSSLength(polygon.Values()[i].Get()));
  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::CreatePolygon(
                                polygon.GetWindRule(), size));
}

InterpolationValue ConvertBasicShape(const BasicShapePolygon& polygon,
                                     double zoom) {
  wtf_size_t size = polygon.Values().size();
  auto list = std::make_unique<InterpolableList>(size);
  for (wtf_size_t i = 0; i < size; i++)
    list->Set(i, ConvertLength(polygon.Values()[i], zoom));
  return InterpolationValue(std::move(list),
                            BasicShapeNonInterpolableValue::CreatePolygon(
                                polygon.GetWindRule(), size));
}

std::unique_ptr<InterpolableValue> CreateNeutralValue(
    const BasicShapeNonInterpolableValue& non_interpolable_value) {
  auto list = std::make_unique<InterpolableList>(non_interpolable_value.size());
  for (wtf_size_t i = 0; i < non_interpolable_value.size(); i++)
    list->Set(i, InterpolableLength::CreateNeutral());
  return std::move(list);
}

scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const BasicShapeNonInterpolableValue& non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  scoped_refptr<BasicShapePolygon> polygon = BasicShapePolygon::Create();
  polygon->SetWindRule(non_interpolable_value.GetWindRule());
  const auto& list = To<InterpolableList>(interpolable_value);
  wtf_size_t size = non_interpolable_value.size();
  DCHECK_EQ(list.length(), size);
  DCHECK_EQ(size % 2, 0U);
  for (wtf_size_t i = 0; i < size; i += 2) {
    polygon->AppendPoint(
        To<InterpolableLength>(*list.Get(i))
            .CreateLength(conversion_data, Length::ValueRange::kAll),
        To<InterpolableLength>(*list.Get(i + 1))
            .CreateLength(conversion_data, Length::ValueRange::kAll));
  }
  return polygon;
}

}  // namespace polygon_functions

}  // namespace

InterpolationValue basic_shape_interpolation_functions::MaybeConvertCSSValue(
    const CSSValue& value) {
  if (auto* circle_value =
          DynamicTo<cssvalue::CSSBasicShapeCircleValue>(value)) {
    return circle_functions::ConvertCSSValue(*circle_value);
  }

  if (auto* ellipse_value =
          DynamicTo<cssvalue::CSSBasicShapeEllipseValue>(value)) {
    return ellipse_functions::ConvertCSSValue(*ellipse_value);
  }
  if (auto* inset_value = DynamicTo<cssvalue::CSSBasicShapeInsetValue>(value)) {
    return rect_common_functions::ConvertCSSValue(*inset_value);
  }
  if (auto* rect_value = DynamicTo<cssvalue::CSSBasicShapeRectValue>(value)) {
    return rect_common_functions::ConvertCSSValue(*rect_value);
  }
  if (auto* xywh_value = DynamicTo<cssvalue::CSSBasicShapeXYWHValue>(value)) {
    return xywh_functions::ConvertCSSValue(*xywh_value);
  }
  if (auto* polygon_value =
          DynamicTo<cssvalue::CSSBasicShapePolygonValue>(value)) {
    return polygon_functions::ConvertCSSValue(*polygon_value);
  }
  return nullptr;
}

InterpolationValue basic_shape_interpolation_functions::MaybeConvertBasicShape(
    const BasicShape* shape,
    double zoom) {
  if (!shape)
    return nullptr;
  switch (shape->GetType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::ConvertBasicShape(To<BasicShapeCircle>(*shape),
                                                 zoom);
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::ConvertBasicShape(To<BasicShapeEllipse>(*shape),
                                                  zoom);
    case BasicShape::kBasicShapeInsetType:
    case BasicShape::kBasicShapeRectType:
      return rect_common_functions::ConvertBasicShape(
          To<BasicShapeRectCommon>(*shape), zoom);
    case BasicShape::kBasicShapeXYWHType:
      return xywh_functions::ConvertBasicShape(To<BasicShapeXYWH>(*shape),
                                               zoom);
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::ConvertBasicShape(To<BasicShapePolygon>(*shape),
                                                  zoom);
    // Handled by PathInterpolationFunction.
    case BasicShape::kStylePathType:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<InterpolableValue>
basic_shape_interpolation_functions::CreateNeutralValue(
    const NonInterpolableValue& untyped_non_interpolable_value) {
  const auto& non_interpolable_value =
      To<BasicShapeNonInterpolableValue>(untyped_non_interpolable_value);
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeInsetType:
    case BasicShape::kBasicShapeRectType:
      return rect_common_functions::CreateNeutralValue();
    case BasicShape::kBasicShapeXYWHType:
      return xywh_functions::CreateNeutralValue();
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::CreateNeutralValue(non_interpolable_value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool basic_shape_interpolation_functions::ShapesAreCompatible(
    const NonInterpolableValue& a,
    const NonInterpolableValue& b) {
  return To<BasicShapeNonInterpolableValue>(a).IsCompatibleWith(
      To<BasicShapeNonInterpolableValue>(b));
}

scoped_refptr<BasicShape> basic_shape_interpolation_functions::CreateBasicShape(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  const auto& non_interpolable_value =
      To<BasicShapeNonInterpolableValue>(untyped_non_interpolable_value);
  switch (non_interpolable_value.GetShapeType()) {
    case BasicShape::kBasicShapeCircleType:
      return circle_functions::CreateBasicShape(interpolable_value,
                                                conversion_data);
    case BasicShape::kBasicShapeEllipseType:
      return ellipse_functions::CreateBasicShape(interpolable_value,
                                                 conversion_data);
    case BasicShape::kBasicShapeInsetType:
    case BasicShape::kBasicShapeRectType:
      return rect_common_functions::CreateBasicShape(
          non_interpolable_value.GetShapeType(), interpolable_value,
          conversion_data);
    case BasicShape::kBasicShapeXYWHType:
      return xywh_functions::CreateBasicShape(interpolable_value,
                                              conversion_data);
    case BasicShape::kBasicShapePolygonType:
      return polygon_functions::CreateBasicShape(
          interpolable_value, non_interpolable_value, conversion_data);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
