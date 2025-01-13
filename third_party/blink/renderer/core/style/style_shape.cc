// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_shape.h"

#include <variant>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/svg/svg_path_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "ui/gfx/geometry/size.h"

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

  void operator()(const StyleShape::MoveToSegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegMoveToAbs);
  }

  void operator()(const StyleShape::MoveBySegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegMoveToRel);
  }

  void operator()(const StyleShape::LineToSegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToAbs);
  }

  void operator()(const StyleShape::LineBySegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToRel);
  }

  void operator()(const StyleShape::HLineToSegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToHorizontalAbs);
  }

  void operator()(const StyleShape::HLineBySegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToHorizontalRel);
  }

  void operator()(const StyleShape::VLineToSegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToVerticalAbs);
  }

  void operator()(const StyleShape::VLineBySegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegLineToVerticalRel);
  }

  void operator()(const StyleShape::ArcToSegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegArcAbs);
  }
  void operator()(const StyleShape::ArcBySegment& segment) {
    Emit(segment, SVGPathSegType::kPathSegArcRel);
  }
  void operator()(const StyleShape::CloseSegment&) {
    builder.EmitSegment({.command = SVGPathSegType::kPathSegClosePath});
  }

 private:
  void Emit(const StyleShape::SegmentWithTargetPoint& segment,
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

  void Emit(const StyleShape::ArcSegment& segment, SVGPathSegType command) {
    PathSegmentData arc_data{
        .command = command,
        .target_point = PointForLengthPoint(segment.target_point, box_size),
        .arc_sweep = segment.sweep,
        .arc_large = segment.large};
    gfx::SizeF radius = SizeForLengthSize(segment.radius, box_size);
    arc_data.SetArcRadiusX(radius.width());
    arc_data.SetArcRadiusY(radius.height());
    arc_data.SetArcAngle(segment.angle);
    builder.EmitSegment(arc_data);
  }

  SVGPathBuilder& builder;
  const gfx::SizeF& box_size;
};
}  // namespace

void StyleShape::GetPath(Path& path,
                         const gfx::RectF& box_rect,
                         float zoom) const {
  SVGPathBuilder builder(path);
  builder.EmitSegment(
      {.command = SVGPathSegType::kPathSegMoveToAbs,
       .target_point = PointForLengthPoint(origin_, box_rect.size())});

  SegmentVisitor visitor(builder, box_rect.size());
  for (const Segment& segment : segments_) {
    std::visit(visitor, segment);
  }
  // TODO(crbug.com/384870258): retain an LRU size->path cache.
  path.Translate(box_rect.OffsetFromOrigin());
}

}  // namespace blink
