// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_shape.h"

#include <numbers>
#include <variant>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/svg/svg_path_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

StyleShape::StyleShape(WindRule wind_rule,
                       const LengthPoint& origin,
                       Vector<Segment> segments)
    : wind_rule_(wind_rule), origin_(origin), segments_(std::move(segments)) {}

bool StyleShape::IsEqualAssumingSameType(const BasicShape& o) const {
  const StyleShape& other = To<StyleShape>(o);
  return wind_rule_ == other.wind_rule_ && origin_ == other.origin_ &&
         segments_ == other.segments_;
}

namespace {
class SegmentVisitor {
  STACK_ALLOCATED();

 public:
  SegmentVisitor(SVGPathBuilder& path_builder, const gfx::SizeF& size)
      : builder(path_builder), box_size(size) {}

  template <typename T>
  void operator()(const T& segment) {
    Emit(segment, T::kSegType);
  }
  void operator()(const StyleShape::CloseSegment&) {
    builder.EmitSegment({.command = SVGPathSegType::kPathSegClosePath});
  }

 private:
  gfx::PointF PointForControlPoint(
      const StyleShape::ControlPoint& control_point,
      gfx::PointF start,
      gfx::PointF end) const {
    gfx::PointF point = PointForLengthPoint(control_point.point, box_size);
    switch (control_point.origin) {
      case StyleShape::ControlPoint::Origin::kReferenceBox:
        return point;
      case StyleShape::ControlPoint::Origin::kSegmentStart:
        return point + start.OffsetFromOrigin();
      case StyleShape::ControlPoint::Origin::kSegmentEnd:
        return point + end.OffsetFromOrigin();
    }
  }

  template <size_t NumControlPoints, SVGPathSegType T>
  void Emit(const StyleShape::CurveSegment<NumControlPoints, T>& segment,
            SVGPathSegType command) {
    gfx::PointF segment_start = builder.CurrentPoint();
    gfx::PointF target_point =
        PointForLengthPoint(segment.target_point, box_size);
    bool is_absolute = IsAbsolutePathSegType(command);
    gfx::PointF segment_end =
        is_absolute ? target_point
                    : (segment_start + target_point.OffsetFromOrigin());

    gfx::PointF point1 = PointForControlPoint(segment.control_points.at(0),
                                              segment_start, segment_end);
    PathSegmentData data{.command = ToAbsolutePathSegType(command),
                         .target_point = segment_end};

    if (T == SVGPathSegType::kPathSegCurveToCubicSmoothAbs ||
        T == SVGPathSegType::kPathSegCurveToCubicSmoothRel) {
      data.point2 = point1;
    } else {
      data.point1 = point1;
    }

    if (NumControlPoints == 2) {
      data.point2 = PointForControlPoint(segment.control_points.at(1),
                                         segment_start, segment_end);
    }

    builder.EmitSegment(data);
  }

  template <SVGPathSegType T>
  void Emit(const StyleShape::SegmentWithTargetPoint<T>& segment,
            SVGPathSegType command) {
    builder.EmitSegment(
        {.command = command,
         .target_point = PointForLengthPoint(segment.target_point, box_size)});
  }

  void Emit(const StyleShape::HLineSegment& segment, SVGPathSegType command) {
    builder.EmitSegment(
        {.command = command,
         .target_point = {FloatValueForLength(segment.x, box_size.width()),
                          0}});
  }
  void Emit(const StyleShape::VLineSegment& segment, SVGPathSegType command) {
    builder.EmitSegment(
        {.command = command,
         .target_point = {0,
                          FloatValueForLength(segment.y, box_size.height())}});
  }

  template <SVGPathSegType T>
  void Emit(const StyleShape::ArcSegment<T>& segment, SVGPathSegType command) {
    PathSegmentData arc_data{
        .command = command,
        .target_point = PointForLengthPoint(segment.target_point, box_size),
        .arc_sweep = segment.sweep,
        .arc_large = segment.large};
    // https://drafts.csswg.org/css-shapes-2/#direction-agnostic-size:
    // The direction-agnostic size of a box is equal to the length of the
    // diagonal of the box, divided by sqrt(2).
    const float direction_agnostic_size = FloatValueForLength(
        segment.direction_agnostic_radius,
        std::hypot(box_size.width(), box_size.height()) / std::numbers::sqrt2);
    arc_data.SetArcRadiusX(
        FloatValueForLength(segment.radius.Width(), box_size.width()) +
        direction_agnostic_size);
    arc_data.SetArcRadiusY(
        FloatValueForLength(segment.radius.Height(), box_size.height()) +
        direction_agnostic_size);
    arc_data.SetArcAngle(segment.angle);
    builder.EmitSegment(arc_data);
  }

  SVGPathBuilder& builder;
  const gfx::SizeF& box_size;
};
}  // namespace

Path StyleShape::GetPath(const gfx::RectF& box_rect,
                         float /*zoom*/,
                         float path_scale) const {
  SVGPathBuilder builder(GetWindRule());

  builder.EmitSegment(
      {.command = SVGPathSegType::kPathSegMoveToAbs,
       .target_point = PointForLengthPoint(origin_, box_rect.size())});

  SegmentVisitor visitor(builder, box_rect.size());
  for (const Segment& segment : segments_) {
    std::visit(visitor, segment);
  }

  // TODO(crbug.com/384870258): retain an LRU size->path cache.
  const gfx::Vector2dF offset = box_rect.OffsetFromOrigin();
  builder.Transform(
      AffineTransform::Translation(offset.x(), offset.y()).Scale(path_scale));

  return builder.Finalize();
}

}  // namespace blink
