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
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css_position_axis_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_shape_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/shape_functions.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/style_shape.h"
#include "third_party/blink/renderer/core/svg/svg_path_blender.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/point_f.h"

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
//    for curves, 4 lengths + 3 numbers for arc, 2 lengths for the rest.
// Converting to/from this interpolation value consists of either "writing" to
// this stream from a CSSValue/StyleShape, or "reading" from it when converting
// it to the end result (a StyleShape).

class ShapeNonInterpolableValue : public NonInterpolableValue {
 public:
  struct SegmentParams {
    SVGPathSegType type;
    StyleShape::ControlPoint::Origin control_point_origin_1;
    StyleShape::ControlPoint::Origin control_point_origin_2;
    bool operator==(const SegmentParams&) const = default;
  };

  ShapeNonInterpolableValue(WindRule wind_rule, Vector<SegmentParams>&& params)
      : params_(params), wind_rule_(wind_rule) {}
  ~ShapeNonInterpolableValue() override = default;

  const Vector<SegmentParams>& GetParams() const { return params_; }
  WindRule GetWindRule() const { return wind_rule_; }

  virtual std::optional<GeometryBox> GetGeometryBox() const {
    return std::nullopt;
  }
  virtual std::optional<CoordBox> GetCoordBox() const { return std::nullopt; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  Vector<SegmentParams> params_;
  WindRule wind_rule_;
};

class CoordBoxShapeNonInterpolableValue final
    : public ShapeNonInterpolableValue {
 public:
  explicit CoordBoxShapeNonInterpolableValue(WindRule wind_rule,
                                             Vector<SegmentParams>&& params,
                                             CoordBox coord_box)
      : ShapeNonInterpolableValue(wind_rule, std::move(params)),
        coord_box_(coord_box) {}
  ~CoordBoxShapeNonInterpolableValue() override = default;

  std::optional<CoordBox> GetCoordBox() const final { return coord_box_; }

 private:
  CoordBox coord_box_;
};

class GeometryBoxShapeNonInterpolableValue final
    : public ShapeNonInterpolableValue {
 public:
  explicit GeometryBoxShapeNonInterpolableValue(WindRule wind_rule,
                                                Vector<SegmentParams>&& params,
                                                GeometryBox geometry_box)
      : ShapeNonInterpolableValue(wind_rule, std::move(params)),
        geometry_box_(geometry_box) {}
  ~GeometryBoxShapeNonInterpolableValue() override = default;

  std::optional<GeometryBox> GetGeometryBox() const final {
    return geometry_box_;
  }

 private:
  GeometryBox geometry_box_;
};

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
    interpolable_values.push_back(MakeGarbageCollected<InterpolableNumber>(
        segment.angle, CSSPrimitiveValue::UnitType::kDegrees));
    interpolable_values.push_back(InterpolableLength::MaybeConvertLength(
        segment.radius.Width(), property_, zoom_, std::nullopt));
    interpolable_values.push_back(InterpolableLength::MaybeConvertLength(
        segment.radius.Height(), property_, zoom_, std::nullopt));
    interpolable_values.push_back(InterpolableLength::MaybeConvertLength(
        segment.direction_agnostic_radius, property_, zoom_, std::nullopt));
    interpolable_values.push_back(
        MakeGarbageCollected<InterpolableNumber>(segment.large ? 1.f : 0.f));
    interpolable_values.push_back(
        MakeGarbageCollected<InterpolableNumber>(segment.sweep ? 1.f : 0.f));
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

ShapeNonInterpolableValue* MakeShapeNonInterpolableValue(
    WindRule wind_rule,
    Vector<ShapeNonInterpolableValue::SegmentParams>&& params,
    const CSSProperty& property,
    std::optional<GeometryBox> geometry_box,
    std::optional<CoordBox> coord_box) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kClipPath:
      CHECK(geometry_box.has_value());
      return MakeGarbageCollected<GeometryBoxShapeNonInterpolableValue>(
          wind_rule, std::move(params), *geometry_box);
    case CSSPropertyID::kOffsetPath: {
      CHECK(coord_box.has_value());
      return MakeGarbageCollected<CoordBoxShapeNonInterpolableValue>(
          wind_rule, std::move(params), *coord_box);
    }
    default:
      NOTREACHED();
  }
}

InterpolationValue ConvertPath(const StylePath* style_path,
                               const CSSProperty& property,
                               std::optional<GeometryBox> geometry_box,
                               std::optional<CoordBox> coord_box) {
  if (!style_path) {
    return nullptr;
  }

  CHECK(!style_path->ByteStream().IsEmpty());

  HeapVector<Member<InterpolableValue>> interpolable_segments;
  Vector<ShapeNonInterpolableValue::SegmentParams> non_interpolable_segments;
  SVGPathByteStreamSource path_source(style_path->ByteStream());

  auto WriteLength = [&](double value) {
    interpolable_segments.push_back(InterpolableLength::CreatePixels(value));
  };

  auto WritePoint = [&](const gfx::PointF& point) {
    WriteLength(point.x());
    WriteLength(point.y());
  };

  // The first command is always a move-to (M)
  PathSegmentData first_segment = path_source.ParseSegment();
  WritePoint(first_segment.target_point);

  while (path_source.HasMoreData()) {
    PathSegmentData segment = path_source.ParseSegment();
    ShapeNonInterpolableValue::SegmentParams params{segment.command};
    StyleShape::ControlPoint::Origin control_point_origin =
        IsAbsolutePathSegType(segment.command)
            ? StyleShape::ControlPoint::Origin::kReferenceBox
            : StyleShape::ControlPoint::Origin::kSegmentStart;
    switch (segment.command) {
      case SVGPathSegType::kPathSegMoveToAbs:
      case SVGPathSegType::kPathSegMoveToRel:
      case SVGPathSegType::kPathSegLineToAbs:
      case SVGPathSegType::kPathSegLineToRel:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothRel:
        WritePoint(segment.target_point);
        break;
      case SVGPathSegType::kPathSegLineToHorizontalAbs:
      case SVGPathSegType::kPathSegLineToHorizontalRel:
        WriteLength(segment.target_point.x());
        break;
      case SVGPathSegType::kPathSegLineToVerticalAbs:
      case SVGPathSegType::kPathSegLineToVerticalRel:
        WriteLength(segment.target_point.y());
        break;
      case SVGPathSegType::kPathSegCurveToCubicAbs:
      case SVGPathSegType::kPathSegCurveToCubicRel:
        WritePoint(segment.target_point);
        WritePoint(segment.point1);
        WritePoint(segment.point2);
        params.control_point_origin_1 = params.control_point_origin_2 =
            control_point_origin;
        break;
      case SVGPathSegType::kPathSegCurveToQuadraticAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticRel:
        WritePoint(segment.target_point);
        WritePoint(segment.point1);
        params.control_point_origin_1 = control_point_origin;
        break;
      case SVGPathSegType::kPathSegCurveToCubicSmoothAbs:
      case SVGPathSegType::kPathSegCurveToCubicSmoothRel:
        WritePoint(segment.target_point);
        WritePoint(segment.point2);
        params.control_point_origin_1 = control_point_origin;
        break;
      case SVGPathSegType::kPathSegArcAbs:
      case SVGPathSegType::kPathSegArcRel:
        WritePoint(segment.target_point);
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(
                segment.ArcAngle(), CSSPrimitiveValue::UnitType::kDegrees));
        WriteLength(segment.ArcRadiusX());
        WriteLength(segment.ArcRadiusY());
        WriteLength(0);
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(
                segment.LargeArcFlag() ? 1.f : 0.f));
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(
                segment.SweepFlag() ? 1.f : 0.f));
        break;
      case SVGPathSegType::kPathSegClosePath:
        break;
      case SVGPathSegType::kPathSegUnknown:
        NOTREACHED();
    }

    non_interpolable_segments.push_back(params);
  }

  ShapeNonInterpolableValue* non_interpolable = MakeShapeNonInterpolableValue(
      style_path->GetWindRule(), std::move(non_interpolable_segments), property,
      geometry_box, coord_box);

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(interpolable_segments)),
      non_interpolable);
}
InterpolationValue ConvertShape(const StyleShape* style_shape,
                                const CSSProperty& property,
                                float zoom,
                                std::optional<GeometryBox> geometry_box,
                                std::optional<CoordBox> coord_box) {
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

  ShapeNonInterpolableValue* non_interpolable = MakeShapeNonInterpolableValue(
      style_shape->GetWindRule(), std::move(non_interpolable_segments),
      property, geometry_box, coord_box);

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(interpolable_segments)),
      non_interpolable);
}

InterpolationValue ConvertShapeOrPath(const BasicShape* shape,
                                      const CSSProperty& property,
                                      float zoom,
                                      std::optional<GeometryBox> geometry_box,
                                      std::optional<CoordBox> coord_box) {
  if (auto* style_shape = DynamicTo<StyleShape>(shape)) {
    return ConvertShape(style_shape, property, zoom, geometry_box, coord_box);
  }
  return ConvertPath(To<StylePath>(shape), property, geometry_box, coord_box);
}

class UnderlyingShapeConversionChecker final
    : public InterpolationType::ConversionChecker {
 public:
  explicit UnderlyingShapeConversionChecker(const InterpolationValue& value)
      : value_(&To<ShapeNonInterpolableValue>(*value.non_interpolable_value)) {}
  ~UnderlyingShapeConversionChecker() final = default;

  void Trace(Visitor* visitor) const override {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(value_);
  }

  bool IsValid(const CSSInterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    const auto& u =
        To<ShapeNonInterpolableValue>(*underlying.non_interpolable_value);
    return value_->GetWindRule() == u.GetWindRule() &&
           u.GetParams() == value_->GetParams() &&
           u.GetGeometryBox() == value_->GetGeometryBox() &&
           u.GetCoordBox() == value_->GetCoordBox();
  }

 private:
  Member<const ShapeNonInterpolableValue> value_;
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
    double angle = To<InterpolableNumber>(*value_list_.Get(index_++))
                       .Value(conversion_data_);
    Length radius_x = ReadLength();
    Length radius_y = ReadLength();
    Length direction_agnostic_radius = ReadLength();
    double arc_large = To<InterpolableNumber>(*value_list_.Get(index_++))
                           .Value(conversion_data_);
    double arc_sweep = To<InterpolableNumber>(*value_list_.Get(index_++))
                           .Value(conversion_data_);
    return T{{{target_point},
              angle,
              LengthSize(radius_x, radius_y),
              direction_agnostic_radius,
              arc_large > 0.f,
              arc_sweep > 0.f}};
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

// Returns the property's shape() value along with its boxes.
// If the property's value is not a shape(), returns default-constructed info.
struct ShapeOrPathInfo {
  STACK_ALLOCATED();

 public:
  const BasicShape* shape = nullptr;
  std::optional<GeometryBox> geometry_box;
  std::optional<CoordBox> coord_box;
  ShapeOrPathInfo() = default;
  ShapeOrPathInfo(const BasicShape* s,
                  std::optional<GeometryBox> g,
                  std::optional<CoordBox> c)
      : shape(s), geometry_box(g), coord_box(c) {}
};

const ShapeOrPathInfo GetShapeOrPath(const CSSProperty& property,
                                     const ComputedStyle& style) {
  const BasicShape* shape = nullptr;
  std::optional<GeometryBox> geometry_box;
  std::optional<CoordBox> coord_box;
  switch (property.PropertyID()) {
    case CSSPropertyID::kClipPath: {
      auto* operation = DynamicTo<ShapeClipPathOperation>(style.ClipPath());
      if (!operation) {
        return ShapeOrPathInfo();
      }
      shape = operation->GetBasicShape();
      geometry_box = operation->GetGeometryBox();
      break;
    }
    case CSSPropertyID::kOffsetPath: {
      auto* operation = DynamicTo<ShapeOffsetPathOperation>(style.OffsetPath());
      if (!operation) {
        return ShapeOrPathInfo();
      }
      shape = &operation->GetBasicShape();
      coord_box = operation->GetCoordBox();
      break;
    }
    default:
      NOTREACHED();
  }
  if (IsA<StylePath>(shape) || IsA<StyleShape>(shape)) {
    return ShapeOrPathInfo(shape, geometry_box, coord_box);
  }
  return ShapeOrPathInfo();
}

class InheritedShapeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedShapeChecker(const CSSProperty& property, const BasicShape* shape)
      : property_(property), shape_(shape) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(shape_);
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return GetShapeOrPath(property_, *state.ParentStyle()).shape ==
           shape_.Get();
  }

  const CSSProperty& property_;
  const Member<const BasicShape> shape_;
};

bool BoxesMatches(const CSSPropertyID& property_id,
                  const ShapeNonInterpolableValue& value1,
                  const ShapeNonInterpolableValue& value2) {
  switch (property_id) {
    case CSSPropertyID::kClipPath:
      return value1.GetGeometryBox() == value2.GetGeometryBox();
    case CSSPropertyID::kOffsetPath:
      return value1.GetCoordBox() == value2.GetCoordBox();
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
BasicShape* CSSShapeInterpolationType::CreateShape(
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
  return MakeGarbageCollected<StyleShape>(
      shape_non_interpolable_value.GetWindRule(), reader.Origin(),
      std::move(segments));
}

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
  BasicShape* shape = CreateShape(interpolable_value, non_interpolable_value,
                                  state.CssToLengthConversionData());
  const auto* shape_non_interpolable =
      non_interpolable_value
          ? &To<ShapeNonInterpolableValue>(*non_interpolable_value)
          : nullptr;
  switch (CssProperty().PropertyID()) {
    case CSSPropertyID::kClipPath: {
      GeometryBox geometry_box = GeometryBox::kBorderBox;
      if (shape_non_interpolable) {
        if (auto maybe_geometry = shape_non_interpolable->GetGeometryBox()) {
          geometry_box = *maybe_geometry;
        }
      }
      state.StyleBuilder().SetClipPath(
          shape ? MakeGarbageCollected<ShapeClipPathOperation>(shape,
                                                               geometry_box)
                : nullptr);
      break;
    }
    case CSSPropertyID::kOffsetPath: {
      CoordBox coord_box = CoordBox::kBorderBox;
      if (shape_non_interpolable) {
        if (auto maybe_coord = shape_non_interpolable->GetCoordBox()) {
          coord_box = *maybe_coord;
        }
      }
      state.StyleBuilder().SetOffsetPath(
          shape
              ? MakeGarbageCollected<ShapeOffsetPathOperation>(shape, coord_box)
              : nullptr);
      break;
    }
    default:
      NOTREACHED();
  }
}

void CSSShapeInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const auto& value1 = To<ShapeNonInterpolableValue>(
      *underlying_value_owner.Value().non_interpolable_value);
  const auto& value2 =
      To<ShapeNonInterpolableValue>(*value.non_interpolable_value);
  bool boxes_match = BoxesMatches(CssProperty().PropertyID(), value1, value2);
  if (value1.GetWindRule() != value2.GetWindRule() ||
      value1.GetParams() != value2.GetParams() || !boxes_match) {
    // Fallback to discrete replacement when non-interpolable parts differ.
    underlying_value_owner.Set(this, value);
    return;
  }
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
  underlying_value_owner.MutableValue().non_interpolable_value = &value1;
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingShapeConversionChecker>(underlying));

  HeapVector<Member<InterpolableValue>> values;
  auto WriteLength = [&](size_t number = 1) {
    for (size_t i = 0; i < number; ++i) {
      values.push_back(InterpolableLength::CreateNeutral());
    }
  };

  WriteLength(2);

  for (const auto& params :
       To<ShapeNonInterpolableValue>(*underlying.non_interpolable_value)
           .GetParams()) {
    switch (params.type) {
      case SVGPathSegType::kPathSegLineToAbs:
      case SVGPathSegType::kPathSegLineToRel:
      case SVGPathSegType::kPathSegMoveToAbs:
      case SVGPathSegType::kPathSegMoveToRel:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticSmoothRel:
        WriteLength(2);
        break;
      case SVGPathSegType::kPathSegLineToHorizontalAbs:
      case SVGPathSegType::kPathSegLineToHorizontalRel:
      case SVGPathSegType::kPathSegLineToVerticalAbs:
      case SVGPathSegType::kPathSegLineToVerticalRel:
        WriteLength(1);
        break;
      case SVGPathSegType::kPathSegClosePath:
        break;
      case SVGPathSegType::kPathSegCurveToCubicAbs:
      case SVGPathSegType::kPathSegCurveToCubicRel: {
        WriteLength(6);
        break;
      }
      case SVGPathSegType::kPathSegCurveToQuadraticAbs:
      case SVGPathSegType::kPathSegCurveToQuadraticRel:
      case SVGPathSegType::kPathSegCurveToCubicSmoothAbs:
      case SVGPathSegType::kPathSegCurveToCubicSmoothRel:
        WriteLength(4);
        break;
      case SVGPathSegType::kPathSegArcAbs:
      case SVGPathSegType::kPathSegArcRel: {
        WriteLength(2);
        values.push_back(*MakeGarbageCollected<InterpolableNumber>(
            0, CSSPrimitiveValue::UnitType::kDegrees));
        WriteLength(2);
        values.push_back(*MakeGarbageCollected<InterpolableNumber>(0));
        values.push_back(*MakeGarbageCollected<InterpolableNumber>(0));
        break;
      }
      case SVGPathSegType::kPathSegUnknown:
        NOTREACHED();
    }
  }

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(values)),
      underlying.non_interpolable_value);
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  // There is currently no property that has an initial shape().
  return nullptr;
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  auto info = GetShapeOrPath(CssProperty(), *state.ParentStyle());
  if (!info.shape) {
    return nullptr;
  }
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedShapeChecker>(CssProperty(), info.shape));
  return ConvertShapeOrPath(info.shape, CssProperty(),
                            state.ParentStyle()->EffectiveZoom(),
                            info.geometry_box, info.coord_box);
}

InterpolationValue CSSShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  const CSSValue* first_value = &value;
  GeometryBox geometry_box = GeometryBox::kBorderBox;
  CoordBox coord_box = CoordBox::kBorderBox;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    first_value = &list->First();
    if (list->length() == 2) {
      if (const auto* ident = DynamicTo<CSSIdentifierValue>(list->Last())) {
        if (CssProperty().PropertyID() == CSSPropertyID::kClipPath) {
          geometry_box = ident->ConvertTo<GeometryBox>();
        } else if (CssProperty().PropertyID() == CSSPropertyID::kOffsetPath) {
          coord_box = ident->ConvertTo<CoordBox>();
        }
      }
    }
  }

  if (const auto* path = DynamicTo<cssvalue::CSSPathValue>(first_value)) {
    return ConvertPath(path->GetStylePath(), CssProperty(), geometry_box,
                       coord_box);
  }

  const auto* shape_value = DynamicTo<cssvalue::CSSShapeValue>(first_value);

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
        interpolable_segments.push_back(
            arc.HasDirectionAgnosticRadius()
                ? InterpolableLength::CreatePixels(0)
                : InterpolableLength::MaybeConvertCSSValue(
                      arc.Radius().First()));
        interpolable_segments.push_back(
            arc.HasDirectionAgnosticRadius()
                ? InterpolableLength::CreatePixels(0)
                : InterpolableLength::MaybeConvertCSSValue(
                      arc.Radius().Second()));
        interpolable_segments.push_back(
            arc.HasDirectionAgnosticRadius()
                ? InterpolableLength::MaybeConvertCSSValue(arc.Radius().First())
                : InterpolableLength::CreatePixels(0));
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(
                arc.Size() == CSSValueID::kLarge ? 1.f : 0.f));
        interpolable_segments.push_back(
            MakeGarbageCollected<InterpolableNumber>(
                arc.Sweep() == CSSValueID::kCw ? 1.f : 0.f));
        break;
      }
      case SVGPathSegType::kPathSegUnknown:
        NOTREACHED();
    }

    non_interpolable_segments.push_back(params);
  }

  ShapeNonInterpolableValue* non_interpolable = MakeShapeNonInterpolableValue(
      shape_value->GetWindRule(), std::move(non_interpolable_segments),
      CssProperty(), geometry_box, coord_box);

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(std::move(interpolable_segments)),
      non_interpolable);
}

InterpolationValue
CSSShapeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  auto info = GetShapeOrPath(CssProperty(), style);
  return ConvertShapeOrPath(info.shape, CssProperty(), style.EffectiveZoom(),
                            info.geometry_box, info.coord_box);
}

PairwiseInterpolationValue CSSShapeInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  auto& start_params =
      To<ShapeNonInterpolableValue>(*start.non_interpolable_value);
  auto& end_params = To<ShapeNonInterpolableValue>(*end.non_interpolable_value);
  bool boxes_match =
      BoxesMatches(CssProperty().PropertyID(), start_params, end_params);

  if (start_params.GetWindRule() != end_params.GetWindRule() ||
      start_params.GetParams() != end_params.GetParams() || !boxes_match) {
    return nullptr;
  }

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(end.non_interpolable_value));
}

}  // namespace blink
