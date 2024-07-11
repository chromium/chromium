/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static double BisectingAngle(double in_angle, double out_angle) {
  double diff = in_angle - out_angle;
  // WK193015: Prevent bugs due to angles being non-continuous.
  // Use an inclusive lower limit to not produce the same angle for both limits.
  if (diff > 180 || diff <= -180)
    in_angle += 360;
  return (in_angle + out_angle) / 2;
}

void SVGMarkerDataBuilder::Build(const Path& path) {
  path.Apply(this, SVGMarkerDataBuilder::UpdateFromPathElement);
  Flush();
}

void SVGMarkerDataBuilder::UpdateFromPathElement(void* info,
                                                 const PathElement& element) {
  static_cast<SVGMarkerDataBuilder*>(info)->UpdateFromPathElement(element);
}

namespace {

// Path processor that converts an arc segment to a cubic segment with
// equivalent start/end tangents.
class MarkerPathSegmentProcessor : public SVGPathNormalizer {
  STACK_ALLOCATED();

 public:
  MarkerPathSegmentProcessor(SVGPathConsumer* consumer)
      : SVGPathNormalizer(consumer) {}

  void EmitSegment(const PathSegmentData&);

 private:
  Vector<PathSegmentData> DecomposeArc(const PathSegmentData&);
};

Vector<PathSegmentData> MarkerPathSegmentProcessor::DecomposeArc(
    const PathSegmentData& segment) {
  class SegmentCollector : public SVGPathConsumer {
    STACK_ALLOCATED();

   public:
    void EmitSegment(const PathSegmentData& segment) override {
      DCHECK_EQ(segment.command, kPathSegCurveToCubicAbs);
      segments_.push_back(segment);
    }
    Vector<PathSegmentData> ReturnSegments() { return std::move(segments_); }

   private:
    Vector<PathSegmentData> segments_;
  } collector;
  // Temporarily switch to our "collector" to collect the curve segments
  // emitted by DecomposeArcToCubic(), and then switch back to the actual
  // consumer.
  base::AutoReset<SVGPathConsumer*> consumer_scope(&consumer_, &collector);
  DecomposeArcToCubic(current_point_, segment);
  return collector.ReturnSegments();
}

void MarkerPathSegmentProcessor::EmitSegment(
    const PathSegmentData& original_segment) {
  PathSegmentData segment = original_segment;
  // Convert a relative arc to absolute.
  if (segment.command == kPathSegArcRel) {
    segment.command = kPathSegArcAbs;
    segment.target_point += current_point_.OffsetFromOrigin();
  }
  if (segment.command == kPathSegArcAbs) {
    // Decompose and then pass/emit a synthesized cubic with matching tangents.
    Vector<PathSegmentData> decomposed_arc_curves = DecomposeArc(segment);
    if (decomposed_arc_curves.empty()) {
      segment.command = kPathSegLineToAbs;
    } else {
      // Use the first control point from the first curve and the second and
      // last control points from the last curve. (If the decomposition only
      // has one curve then the second line just copies the same point again.)
      segment = decomposed_arc_curves.back();
      segment.point1 = decomposed_arc_curves[0].point1;
    }
  }
  // Invoke the base class to normalize and emit to the consumer
  // (SVGMarkerDataBuilder).
  SVGPathNormalizer::EmitSegment(segment);
}

}  // namespace

void SVGMarkerDataBuilder::Build(const SVGPathByteStream& stream) {
  SVGPathByteStreamSource source(stream);
  MarkerPathSegmentProcessor processor(this);
  svg_path_parser::ParsePath(source, processor);
  Flush();
}

void SVGMarkerDataBuilder::EmitSegment(const PathSegmentData& segment) {
  PathElementType type;
  std::array<gfx::PointF, 3> points;
  size_t count;
  switch (segment.command) {
    case kPathSegClosePath:
      type = kPathElementCloseSubpath;
      count = 0;
      break;
    case kPathSegMoveToAbs:
      type = kPathElementMoveToPoint;
      count = 1;
      points[0] = segment.target_point;
      break;
    case kPathSegLineToAbs:
      type = kPathElementAddLineToPoint;
      count = 1;
      points[0] = segment.target_point;
      break;
    case kPathSegCurveToCubicAbs:
      type = kPathElementAddCurveToPoint;
      count = 3;
      points[0] = segment.point1;
      points[1] = segment.point2;
      points[2] = segment.target_point;
      break;
    default:
      NOTREACHED();
  }
  UpdateFromPathElement({type, base::span(points).first(count)});
}

double SVGMarkerDataBuilder::CurrentAngle(AngleType type) const {
  // For details of this calculation, see:
  // http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
  double in_angle = Rad2deg(in_slope_.SlopeAngleRadians());
  double out_angle = Rad2deg(out_slope_.SlopeAngleRadians());
  switch (type) {
    case kOutbound:
      return out_angle;
    case kBisecting:
      return BisectingAngle(in_angle, out_angle);
    case kInbound:
      return in_angle;
  }
}

SVGMarkerDataBuilder::AngleType SVGMarkerDataBuilder::DetermineAngleType(
    bool ends_subpath) const {
  // If this is closing the path, (re)compute the angle to be the one bisecting
  // the in-slope of the 'close' and the out-slope of the 'move to'.
  if (last_element_type_ == kPathElementCloseSubpath)
    return kBisecting;
  // If this is the end of an open subpath (closed subpaths handled above),
  // use the in-slope.
  if (ends_subpath)
    return kInbound;
  // If |last_element_type_| is a 'move to', apply the same rule as for a
  // "start" marker. If needed we will backpatch the angle later.
  if (last_element_type_ == kPathElementMoveToPoint)
    return kOutbound;
  // Else use the bisecting angle.
  return kBisecting;
}

void SVGMarkerDataBuilder::UpdateAngle(bool ends_subpath) {
  // When closing a subpath, update the current out-slope to be that of the
  // 'move to' command.
  if (last_element_type_ == kPathElementCloseSubpath)
    out_slope_ = last_moveto_out_slope_;
  AngleType type = DetermineAngleType(ends_subpath);
  float angle = ClampTo<float>(CurrentAngle(type));
  // When closing a subpath, backpatch the first marker on that subpath.
  if (last_element_type_ == kPathElementCloseSubpath)
    positions_[last_moveto_index_].angle = angle;
  positions_.back().angle = angle;
}

void SVGMarkerDataBuilder::ComputeQuadTangents(SegmentData& data,
                                               const gfx::PointF& start,
                                               const gfx::PointF& control,
                                               const gfx::PointF& end) {
  data.start_tangent = control - start;
  data.end_tangent = end - control;
  if (data.start_tangent.IsZero())
    data.start_tangent = data.end_tangent;
  else if (data.end_tangent.IsZero())
    data.end_tangent = data.start_tangent;
}

SVGMarkerDataBuilder::SegmentData
SVGMarkerDataBuilder::ExtractPathElementFeatures(
    const PathElement& element) const {
  SegmentData data;
  const base::span<const gfx::PointF> points = element.points;
  switch (element.type) {
    case kPathElementAddCurveToPoint:
      data.position = points[2];
      data.start_tangent = points[0] - origin_;
      data.end_tangent = points[2] - points[1];
      if (data.start_tangent.IsZero())
        ComputeQuadTangents(data, points[0], points[1], points[2]);
      else if (data.end_tangent.IsZero())
        ComputeQuadTangents(data, origin_, points[0], points[1]);
      break;
    case kPathElementAddQuadCurveToPoint:
      data.position = points[1];
      ComputeQuadTangents(data, origin_, points[0], points[1]);
      break;
    case kPathElementMoveToPoint:
    case kPathElementAddLineToPoint:
      data.position = points[0];
      data.start_tangent = data.position - origin_;
      data.end_tangent = data.position - origin_;
      break;
    case kPathElementCloseSubpath: {
      gfx::Vector2dF tangent = subpath_start_ - origin_;
      // If the current point equals the start point of the subpath, and this
      // not a subpath with just a 'moveto', then use the saved tangent from
      // the start of the subpath.
      if (last_element_type_ != kPathElementMoveToPoint && tangent.IsZero()) {
        tangent = last_moveto_out_slope_;
      }
      data.position = subpath_start_;
      data.start_tangent = tangent;
      data.end_tangent = tangent;
      break;
    }
  }
  return data;
}

void SVGMarkerDataBuilder::UpdateFromPathElement(const PathElement& element) {
  SegmentData segment_data = ExtractPathElementFeatures(element);

  // First update the outgoing slope for the previous element.
  out_slope_ = segment_data.start_tangent;

  // Save the out-slope for the new subpath.
  if (last_element_type_ == kPathElementMoveToPoint)
    last_moveto_out_slope_ = out_slope_;

  // Record the angle for the previous element.
  bool starts_new_subpath = element.type == kPathElementMoveToPoint;
  if (!positions_.empty())
    UpdateAngle(starts_new_subpath);

  // Update the incoming slope for this marker position.
  in_slope_ = segment_data.end_tangent;

  // Update marker position.
  origin_ = segment_data.position;

  // If this is a 'move to' segment, save the point for use with 'close', and
  // the the index in the list to allow backpatching the angle on 'close'.
  if (starts_new_subpath) {
    subpath_start_ = element.points[0];
    last_moveto_index_ = positions_.size();
  }

  last_element_type_ = element.type;

  // Output a marker for this element. The angle will be computed at a later
  // stage. Similarly for 'end' markers the marker type will be updated at a
  // later stage.
  SVGMarkerType marker_type = positions_.empty() ? kStartMarker : kMidMarker;
  positions_.push_back(MarkerPosition(marker_type, origin_, 0));
}

void SVGMarkerDataBuilder::Flush() {
  if (positions_.empty())
    return;
  const bool kEndsSubpath = true;
  UpdateAngle(kEndsSubpath);
  // Mark the last marker as 'end'.
  positions_.back().type = kEndMarker;
}

}  // namespace blink
