// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_shape_interpolation_type.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shape_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/shape_functions.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/style_shape.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

namespace blink {

namespace {
// The shape interpolation value is divided to two:
// 1. The non-interpolable value, contains the WindRule + a Vector of
// SegmentParams. Each of these segments
//    contains the type, plus additional segment-specific non-interpolable data
//    for curves/arcs.
// 2. The interpolable value is a single flat list for the whole shape. It's
// like a stream of InterpolableValues,
//    consisting of 2 Lengths for the origin + 0-6 values per segments, mostly
//    InerpolableLength: 0 for Close, 1 length for hline/vLine, 4 or 6 Lengths
//    for curves, 4 lengths + angle for arc, 2 lengths for the rest.
// Converting to/from this interpolation value consists of either "writing" to
// this stream from a CSSValue/StyleShape, or "reading" from it when converting
// it to the end result (a StyleShape).

class ShapeNonInterpolableValue : public NonInterpolableValue {
 public:
  struct SegmentParams {
    SVGPathSegType type;
    bool arc_large;
    bool arc_sweep;
    StyleShape::ControlPoint::Origin control_point_origin_1;
    StyleShape::ControlPoint::Origin control_point_origin_2;
  };

  ~ShapeNonInterpolableValue() override = default;

  static scoped_refptr<ShapeNonInterpolableValue> Create(
      WindRule wind_rule,
      Vector<SegmentParams> params) {
    return base::AdoptRef(
        new ShapeNonInterpolableValue(wind_rule, std::move(params)));
  }

  const Vector<SegmentParams>& GetParams() const { return params_; }
  WindRule GetWindRule() const { return wind_rule_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  ShapeNonInterpolableValue(WindRule wind_rule, Vector<SegmentParams> params)
      : params_(std::move(params)), wind_rule_(wind_rule) {}

  Vector<SegmentParams> params_;
  WindRule wind_rule_;
};

bool ParamsMatch(
    const Vector<ShapeNonInterpolableValue::SegmentParams>& list_a,
    const Vector<ShapeNonInterpolableValue::SegmentParams>& list_b) {
  if (list_a.size() != list_b.size()) {
    return false;
  }
  for (size_t i = 0; i < list_a.size(); ++i) {
    const auto& a = list_a[i];
    const auto& b = list_b[i];

    // We only match curves, as mismatching arcs can still interpolate.
    if (a.type != b.type ||
        a.control_point_origin_1 != b.control_point_origin_1 ||
        a.control_point_origin_2 != b.control_point_origin_2) {
      return false;
    }
  }

  return true;
}

class ShapeSegmentInterpolationBuilder {
  STACK_ALLOCATED();

 public:
  ShapeSegmentInterpolationBuilder(
      HeapVector<Member<InterpolableValue>>& values,
      const CSSProperty& property,
      float zoom,
      const LengthPoint& origin)
      : interpolable_values(values), property_(property), zoom_(zoom) {
    Write(origin);
  }

  template <typename T>
  ShapeNonInterpolableValue::SegmentParams operator()(const T& segment) {
    ShapeNonInterpolableValue::SegmentParams command{T::kSegType};
    Write(segment, command);
    return command;
  }

  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::CubicCurveToSegment& segment) {
    return WriteCurve2<SVGPathSegType::kPathSegCurveToCubicAbs>(segment);
  }
  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::CubicCurveBySegment& segment) {
    return WriteCurve2<SVGPathSegType::kPathSegCurveToCubicRel>(segment);
  }
  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::QuadraticCurveToSegment& segment) {
    return WriteCurve1<SVGPathSegType::kPathSegCurveToQuadraticAbs>(segment);
  }
  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::QuadraticCurveBySegment& segment) {
    return WriteCurve1<SVGPathSegType::kPathSegCurveToQuadraticRel>(segment);
  }
  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::SmoothCubicCurveToSegment& segment) {
    return WriteCurve1<SVGPathSegType::kPathSegCurveToCubicSmoothAbs>(segment);
  }
  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::SmoothCubicCurveBySegment& segment) {
    return WriteCurve1<SVGPathSegType::kPathSegCurveToCubicSmoothRel>(segment);
  }

  ShapeNonInterpolableValue::SegmentParams operator()(
      const StyleShape::CloseSegment&) {
    return {.type = SVGPathSegType::kPathSegClosePath};
  }

 private:
  template <SVGPathSegType Type>
  void Write(const StyleShape::SegmentWithTargetPoint<Type>& segment,
             ShapeNonInterpolableValue::SegmentParams&) {
    Write(segment.target_point);
  }

  void Write(const StyleShape::HLineSegment& segment,
             ShapeNonInterpolableValue::SegmentParams&) {
    Write(segment.x);
  }

  void Write(const StyleShape::VLineSegment& segment,
             ShapeNonInterpolableValue::SegmentParams&) {
    Write(segment.y);
  }

  template <SVGPathSegType Type>
  void Write(const StyleShape::ArcSegment<Type>& segment,
             ShapeNonInterpolableValue::SegmentParams& params) {
    Write(segment.target_point);
    interpolable_values.push_back(
        MakeGarbageCollected<InterpolableNumber>(segment.angle));
    Write(segment.radius.Width());
    Write(segment.radius.Height());
    params.arc_large = segment.large;
    params.arc_sweep = segment.sweep;
  }

  template <SVGPathSegType Type>
  ShapeNonInterpolableValue::SegmentParams WriteCurve1(
      const StyleShape::CurveSegment<1, Type>& segment) {
    Write(segment.target_point);
    Write(segment.control_points.at(0).point);
    return {.type = Type,
            .control_point_origin_1 = segment.control_points.at(0).origin};
  }

  template <SVGPathSegType Type>
  ShapeNonInterpolableValue::SegmentParams WriteCurve2(
      const StyleShape::CurveSegment<2, Type>& segment) {
    Write(segment.target_point);
    Write(segment.control_points.at(0).point);
    Write(segment.control_points.at(1).point);
    return {.type = Type,
            .control_point_origin_1 = segment.control_points.at(0).origin,
            .control_point_origin_2 = segment.control_points.at(1).origin};
  }

  void Write(const Length& length) {
    interpolable_values.push_back(InterpolableLength::MaybeConvertLength(
        length, property_, zoom_, std::nullopt));
  }

  void Write(const LengthPoint& point) {
    Write(point.X());
    Write(point.Y());
  }

  HeapVector<Member<InterpolableValue>>& interpolable_values;
  const CSSProperty& property_;
  float zoom_;
};

InterpolationValue ConvertShape(const StyleShape* style_shape,
                                const CSSProperty& property,
                                float zoom) {
  if (!style_shape) {
    return nullptr;
  }

  HeapVector<Member<InterpolableValue>> interpolable_segments;
  Vector<ShapeNonInterpolableValue::SegmentParams> non_interpolable_segments;
  non_interpolable_segments.ReserveInitialCapacity(
      style_shape->Segments().size());
  ShapeSegmentInterpolationBuilder builder(interpolable_segments, property,
                                           zoom, style_shape->GetOrigin());
  for (const StyleShape::Segment& segment : style_shape->Segments()) {
    non_interpolable_segments.push_back(std::visit(builder, segment));
  }

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(interpolable_segments)),
      ShapeNonInterpolableValue::Create(style_shape->GetWindRule(),
                                        std::move(non_interpolable_segments)));
}

class UnderlyingShapeConversionChecker final
    : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingShapeConversionChecker() final = default;

  static UnderlyingShapeConversionChecker* Create(
      const InterpolationValue& underlying) {
    return MakeGarbageCollected<UnderlyingShapeConversionChecker>(underlying);
  }

  explicit UnderlyingShapeConversionChecker(const InterpolationValue& value)
      : value_(&To<ShapeNonInterpolableValue>(*value.non_interpolable_value)) {}

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return value_->GetWindRule() ==
               To<ShapeNonInterpolableValue>(*underlying.non_interpolable_value)
                   .GetWindRule() &&
           ParamsMatch(
               To<ShapeNonInterpolableValue>(*underlying.non_interpolable_value)
                   .GetParams(),
               value_->GetParams());
  }

 private:
  scoped_refptr<const ShapeNonInterpolableValue> value_;
};

class ShapeInterpolationReader {
  STACK_ALLOCATED();

 public:
  ShapeInterpolationReader(const InterpolableList& list,
                           const CSSToLengthConversionData& conversation_data)
      : value_list_(list), conversion_data_(conversation_data) {
    origin_ = ReadPoint();
  }

  LengthPoint Origin() { return origin_; }

  template <typename SegmentType>
  StyleShape::Segment Read() {
    return SegmentType{ReadPoint()};
  }

  template <typename SegmentType>
  StyleShape::Segment ReadCurve1(
      const ShapeNonInterpolableValue::SegmentParams& params) {
    return SegmentType{{{ReadPoint()},
                        {StyleShape::ControlPoint{params.control_point_origin_1,
                                                  ReadPoint()}}}};
  }

  template <typename SegmentType>
  StyleShape::Segment ReadCurve2(
      const ShapeNonInterpolableValue::SegmentParams& params) {
    return SegmentType{
        {{ReadPoint()},
         {StyleShape::ControlPoint{params.control_point_origin_1, ReadPoint()},
          StyleShape::ControlPoint{params.control_point_origin_2,
                                   ReadPoint()}}}};
  }

  template <typename T>
  StyleShape::Segment ReadArc(
      const ShapeNonInterpolableValue::SegmentParams& params) {
    LengthPoint target_point = ReadPoint();
    double angle = To<InterpolableNumber>(*value_list_.Get(index_))
                       .Value(conversion_data_);
    index_++;
    LengthSize radius(ReadLength(), ReadLength());
    return T{
        {{target_point}, angle, radius, params.arc_large, params.arc_sweep}};
  }

  Length ReadLength() {
    Length length =
        To<InterpolableLength>(*value_list_.Get(index_))
            .CreateLength(conversion_data_, Length::ValueRange::kAll);
    index_++;
    return length;
  }

 private:
  LengthPoint ReadPoint() { return LengthPoint(ReadLength(), ReadLength()); }

  size_t index_ = 0;
  const InterpolableList& value_list_;
  const CSSToLengthConversionData& conversion_data_;
  LengthPoint origin_;
};

scoped_refptr<StyleShape> CreateStyleShape(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  if (!non_interpolable_value) {
    return nullptr;
  }

  const auto& shape_non_interpolable_value =
      To<ShapeNonInterpolableValue>(*non_interpolable_value);

  const auto& value_list = To<InterpolableList>(interpolable_value);

  // At the very least, the value list should contain the origin point.
  CHECK_GE(value_list.length(), 2u);

  Vector<StyleShape::Segment> segments(
      shape_non_interpolable_value.GetParams().size());
  ShapeInterpolationReader reader(value_list, conversion_data);

  std::ranges::transform(
      shape_non_interpolable_value.GetParams(), segments.begin(),
      [&](const ShapeNonInterpolableValue::SegmentParams& params)
          -> StyleShape::Segment {
        switch (params.type) {
          case SVGPathSegType::kPathSegMoveToAbs:
            return reader.Read<StyleShape::MoveToSegment>();
          case SVGPathSegType::kPathSegMoveToRel:
            return reader.Read<StyleShape::MoveBySegment>();
          case SVGPathSegType::kPathSegLineToAbs:
            return reader.Read<StyleShape::LineToSegment>();
          case SVGPathSegType::kPathSegLineToRel:
            return reader.Read<StyleShape::LineBySegment>();
          case SVGPathSegType::kPathSegCurveToQuadraticSmoothAbs:
            return reader.Read<StyleShape::SmoothQuadraticCurveToSegment>();
          case SVGPathSegType::kPathSegCurveToQuadraticSmoothRel:
            return reader.Read<StyleShape::SmoothQuadraticCurveBySegment>();
          case SVGPathSegType::kPathSegLineToHorizontalAbs:
            return StyleShape::HLineToSegment{reader.ReadLength()};
          case SVGPathSegType::kPathSegLineToHorizontalRel:
            return StyleShape::HLineBySegment{reader.ReadLength()};
          case SVGPathSegType::kPathSegLineToVerticalAbs:
            return StyleShape::VLineToSegment{reader.ReadLength()};
          case SVGPathSegType::kPathSegLineToVerticalRel:
            return StyleShape::VLineBySegment{reader.ReadLength()};
          case SVGPathSegType::kPathSegCurveToCubicAbs:
            return reader.ReadCurve2<StyleShape::CubicCurveToSegment>(params);
          case SVGPathSegType::kPathSegCurveToCubicRel:
            return reader.ReadCurve2<StyleShape::CubicCurveBySegment>(params);
          case SVGPathSegType::kPathSegCurveToQuadraticAbs:
            return reader.ReadCurve1<StyleShape::QuadraticCurveToSegment>(
                params);
          case SVGPathSegType::kPathSegCurveToQuadraticRel:
            return reader.ReadCurve1<StyleShape::QuadraticCurveBySegment>(
                params);
          case SVGPathSegType::kPathSegCurveToCubicSmoothAbs:
            return reader.ReadCurve1<StyleShape::SmoothCubicCurveToSegment>(
                params);
          case SVGPathSegType::kPathSegCurveToCubicSmoothRel:
            return reader.ReadCurve1<StyleShape::SmoothCubicCurveBySegment>(
                params);
          case SVGPathSegType::kPathSegArcAbs:
            return reader.ReadArc<StyleShape::ArcToSegment>(params);
          case SVGPathSegType::kPathSegArcRel:
            return reader.ReadArc<StyleShape::ArcBySegment>(params);
          case SVGPathSegType::kPathSegClosePath:
            return StyleShape::CloseSegment{};
          case SVGPathSegType::kPathSegUnknown:
            NOTREACHED();
        }
      });
  return StyleShape::Create(shape_non_interpolable_value.GetWindRule(),
                            reader.Origin(), std::move(segments));
}

// Returns the property's shape() value.
// If the property's value is not a shape(), returns nullptr.
const StyleShape* GetShape(const CSSProperty& property,
                           const ComputedStyle& style) {
  // TODO(crbug.com/389713717) support also offset-path
  CHECK_EQ(property.PropertyID(), CSSPropertyID::kClipPath);
  if (auto* shape = DynamicTo<ShapeClipPathOperation>(style.ClipPath())) {
    return DynamicTo<StyleShape>(shape->GetBasicShape());
  }
  return nullptr;
}

class InheritedShapeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedShapeChecker(const CSSProperty& property,
                        scoped_refptr<const StyleShape> style_shape)
      : property_(property), style_shape_(std::move(style_shape)) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return GetShape(property_, *state.ParentStyle()) == style_shape_.get();
  }

  const CSSProperty& property_;
  const scoped_refptr<const StyleShape> style_shape_;
};

}  // namespace

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ShapeNonInterpolableValue);
template <>
struct DowncastTraits<ShapeNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == ShapeNonInterpolableValue::static_type_;
  }
};

void CSSShapeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  // TODO(crbug.com/389713717) support also offset-path
  CHECK_EQ(CssProperty().PropertyID(), CSSPropertyID::kClipPath);
  auto shape = CreateStyleShape(interpolable_value, non_interpolable_value,
                                state.CssToLengthConversionData());

  // TODO(nrosenthal): Handle geometry box.
  state.StyleBuilder().SetClipPath(
      shape ? MakeGarbageCollected<ShapeClipPathOperation>(
                  shape, GeometryBox::kBorderBox)
            : nullptr);
}

void CSSShapeInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double) const {
  const auto& value1 = To<ShapeNonInterpolableValue>(
      *underlying_value_owner.Value().non_interpolable_value);
  const auto& value2 =
      To<ShapeNonInterpolableValue>(*value.non_interpolable_value);

  DCHECK(ParamsMatch(value1.GetParams(), value2.GetParams()));
  // TODO(crbug.com/384781868) arcs with different size/sweep should default to
  // large/cw.
  underlying_value_owner.MutableValue().non_interpolable_value =
      value.non_interpolable_value.get();
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return nullptr;
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return nullptr;
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  auto* shape = GetShape(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedShapeChecker>(CssProperty(), shape));
  return ConvertShape(shape, CssProperty(),
                      state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  const cssvalue::CSSShapeValue* shape_value = nullptr;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    shape_value = DynamicTo<cssvalue::CSSShapeValue>(list->First());
  } else {
    shape_value = DynamicTo<cssvalue::CSSShapeValue>(value);
  }
  if (!shape_value) {
    return nullptr;
  }

  HeapVector<Member<InterpolableValue>> interpolable_segments;
  Vector<ShapeNonInterpolableValue::SegmentParams> non_interpolable_segments;
  non_interpolable_segments.ReserveInitialCapacity(
      shape_value->Commands().size());

  auto WriteLength = [&](const CSSValue& value) {
    if (const auto* identifier = DynamicTo<CSSIdentifierValue>(value)) {
      switch (identifier->GetValueID()) {
        case CSSValueID::kXStart:
        case CSSValueID::kYStart:
          interpolable_segments.push_back(InterpolableLength::CreatePercent(0));
          return;
        case CSSValueID::kXEnd:
        case CSSValueID::kYEnd:
          interpolable_segments.push_back(
              InterpolableLength::CreatePercent(100));
          return;
        default:
          break;
      }
    }

    interpolable_segments.push_back(
        CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(value)
            .interpolable_value);
    CHECK(interpolable_segments[interpolable_segments.size() - 1]);
  };

  auto WritePair = [&](const CSSValuePair& pair) {
    WriteLength(pair.First());
    WriteLength(pair.Second());
  };

  WritePair(shape_value->GetOrigin());

  for (const cssvalue::CSSShapeCommand* command : shape_value->Commands()) {
    ShapeNonInterpolableValue::SegmentParams params{command->GetType()};

    switch (params.type) {
      case SVGPathSegType::kPathSegLineToAbs:
      case SVGPathSegType::kPathSegLineToRel:
      case SVGPathSegType::kPathSegMoveToAbs:
      case SVGPathSegType::kPathSegMoveToRel:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothRel:
        WritePair(To<CSSValuePair>(command->GetEndPoint()));
        break;
      case SVGPathSegType::kPathSegLineToHorizontalAbs:
      case SVGPathSegType::kPathSegLineToHorizontalRel:
      case SVGPathSegType::kPathSegLineToVerticalAbs:
      case SVGPathSegType::kPathSegLineToVerticalRel:
        WriteLength(command->GetEndPoint());
        break;
      case SVGPathSegType::kPathSegClosePath:
        break;
      case SVGPathSegType::kPathSegCurveToCubicAbs:
      case SVGPathSegType::kPathSegCurveToCubicRel: {
        const auto& curve =
            static_cast<const cssvalue::CSSShapeCurveCommand<2>&>(*command);
        WritePair(To<CSSValuePair>(curve.GetEndPoint()));
        WritePair(*curve.GetControlPoints().at(0).second);
        WritePair(*curve.GetControlPoints().at(1).second);
        params.control_point_origin_1 =
            ToControlPointOrigin(curve.GetControlPoints().at(0).first);
        params.control_point_origin_2 =
            ToControlPointOrigin(curve.GetControlPoints().at(1).first);
        break;
      }
      case SVGPathSegType::kPathSegCurveToQuadraticAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticRel:
      case SVGPathSegType::kPathSegCurveToCubicSmoothAbs:
      case SVGPathSegType::kPathSegCurveToCubicSmoothRel: {
        const auto& curve =
            static_cast<const cssvalue::CSSShapeCurveCommand<1>&>(*command);
        WritePair(To<CSSValuePair>(curve.GetEndPoint()));
        WritePair(*curve.GetControlPoints().at(0).second);
        params.control_point_origin_1 =
            ToControlPointOrigin(curve.GetControlPoints().at(0).first);
        break;
      }
      case SVGPathSegType::kPathSegArcAbs:
      case SVGPathSegType::kPathSegArcRel: {
        const auto& arc =
            static_cast<const cssvalue::CSSShapeArcCommand&>(*command);
        WritePair(To<CSSValuePair>(arc.GetEndPoint()));
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(arc.Angle()));
        WritePair(arc.Radius());
        params.arc_large = arc.Size() == CSSValueID::kLarge;
        params.arc_sweep = arc.Sweep() == CSSValueID::kCw;
        break;
      }
      case SVGPathSegType::kPathSegUnknown:
        NOTREACHED();
    }

    non_interpolable_segments.push_back(params);
  }

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(interpolable_segments)),
      ShapeNonInterpolableValue::Create(shape_value->GetWindRule(),
                                        std::move(non_interpolable_segments)));
}

InterpolationValue
CSSShapeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertShape(GetShape(CssProperty(), style), CssProperty(),
                      style.EffectiveZoom());
}

PairwiseInterpolationValue CSSShapeInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  auto& start_params =
      To<ShapeNonInterpolableValue>(*start.non_interpolable_value);
  auto& end_params = To<ShapeNonInterpolableValue>(*end.non_interpolable_value);

  if (start_params.GetWindRule() != end_params.GetWindRule() ||
      !ParamsMatch(start_params.GetParams(), end_params.GetParams())) {
    return nullptr;
  }

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(end.non_interpolable_value));
}

}  // namespace blink
