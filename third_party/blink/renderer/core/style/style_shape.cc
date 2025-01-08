// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_shape.h"

#include "third_party/blink/renderer/core/svg/svg_path_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

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

void StyleShape::ResolvePath(Path& path, const gfx::SizeF& box_size) const {
  SVGPathBuilder builder(path);
  builder.EmitSegment({.command = SVGPathSegType::kPathSegMoveToAbs,
                       .target_point = PointForLengthPoint(origin_, box_size)});

  for (const Segment& segment : segments_) {
    // TODO(crbug.com/384870259): support other segment types.
    switch (segment.type) {
      case Segment::Type::kMove:
        builder.EmitSegment(
            {.command =
                 segment.end_point_origin == Segment::PointOrigin::kReferenceBox
                     ? SVGPathSegType::kPathSegMoveToAbs
                     : SVGPathSegType::kPathSegMoveToRel,
             .target_point = PointForLengthPoint(segment.end_point, box_size)});
        break;
      case Segment::Type::kLine:
        builder.EmitSegment(
            {.command =
                 segment.end_point_origin == Segment::PointOrigin::kReferenceBox
                     ? SVGPathSegType::kPathSegLineToAbs
                     : SVGPathSegType::kPathSegLineToRel,
             .target_point = PointForLengthPoint(segment.end_point, box_size)});
        break;
      case Segment::Type::kClose:
        builder.EmitSegment({.command = SVGPathSegType::kPathSegClosePath});
        break;
    }
  }
}

void StyleShape::GetPath(Path& path,
                         const gfx::RectF& box_rect,
                         float zoom) const {
  ResolvePath(path, box_rect.size());
  // TODO(crbug.com/384870258): retain an LRU size->path cache.
  path.Translate(box_rect.OffsetFromOrigin());
}

}  // namespace blink
