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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_MARKER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_MARKER_DATA_H_

#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

enum SVGMarkerType { kStartMarker, kMidMarker, kEndMarker };

struct MarkerPosition {
  DISALLOW_NEW();
  MarkerPosition(SVGMarkerType use_type,
                 const FloatPoint& use_origin,
                 float use_angle)
      : type(use_type), origin(use_origin), angle(use_angle) {}

  SVGMarkerType type;
  FloatPoint origin;
  float angle;
};

class LayoutSVGResourceMarker;

class SVGMarkerData {
  STACK_ALLOCATED();

 public:
  SVGMarkerData(Vector<MarkerPosition>& positions, bool auto_start_reverse)
      : positions_(positions),
        element_index_(0),
        auto_start_reverse_(auto_start_reverse) {}

  static void UpdateFromPathElement(void* info, const PathElement* element) {
    static_cast<SVGMarkerData*>(info)->UpdateFromPathElement(*element);
  }

  void PathIsDone() {
    float angle = clampTo<float>(CurrentAngle(kEndMarker));
    positions_.push_back(MarkerPosition(kEndMarker, origin_, angle));
  }

  static inline LayoutSVGResourceMarker* MarkerForType(
      const SVGMarkerType& type,
      LayoutSVGResourceMarker* marker_start,
      LayoutSVGResourceMarker* marker_mid,
      LayoutSVGResourceMarker* marker_end) {
    switch (type) {
      case kStartMarker:
        return marker_start;
      case kMidMarker:
        return marker_mid;
      case kEndMarker:
        return marker_end;
    }

    NOTREACHED();
    return nullptr;
  }

 private:
  static double BisectingAngle(double in_angle, double out_angle) {
    // WK193015: Prevent bugs due to angles being non-continuous.
    if (fabs(in_angle - out_angle) > 180)
      in_angle += 360;
    return (in_angle + out_angle) / 2;
  }

  double CurrentAngle(SVGMarkerType type) const {
    // For details of this calculation, see:
    // http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
    double in_angle = rad2deg(FloatPoint(in_slope_).SlopeAngleRadians());
    double out_angle = rad2deg(FloatPoint(out_slope_).SlopeAngleRadians());

    switch (type) {
      case kStartMarker:
        if (auto_start_reverse_)
          out_angle += 180;
        return out_angle;
      case kMidMarker:
        return BisectingAngle(in_angle, out_angle);
      case kEndMarker:
        return in_angle;
    }

    NOTREACHED();
    return 0;
  }

  struct SegmentData {
    FloatSize start_tangent;  // Tangent in the start point of the segment.
    FloatSize end_tangent;    // Tangent in the end point of the segment.
    FloatPoint position;      // The end point of the segment.
  };

  static void ComputeQuadTangents(SegmentData& data,
                                  const FloatPoint& start,
                                  const FloatPoint& control,
                                  const FloatPoint& end) {
    data.start_tangent = control - start;
    data.end_tangent = end - control;
    if (data.start_tangent.IsZero())
      data.start_tangent = data.end_tangent;
    else if (data.end_tangent.IsZero())
      data.end_tangent = data.start_tangent;
  }

  SegmentData ExtractPathElementFeatures(const PathElement& element) const {
    SegmentData data;
    const FloatPoint* points = element.points;
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
      case kPathElementCloseSubpath:
        data.position = subpath_start_;
        data.start_tangent = data.position - origin_;
        data.end_tangent = data.position - origin_;
        break;
    }
    return data;
  }

  void UpdateFromPathElement(const PathElement& element) {
    SegmentData segment_data = ExtractPathElementFeatures(element);

    // First update the outgoing slope for the previous element.
    out_slope_ = segment_data.start_tangent;

    // Record the marker for the previous element.
    if (element_index_ > 0) {
      SVGMarkerType marker_type =
          element_index_ == 1 ? kStartMarker : kMidMarker;
      float angle = clampTo<float>(CurrentAngle(marker_type));
      positions_.push_back(MarkerPosition(marker_type, origin_, angle));
    }

    // Update the incoming slope for this marker position.
    in_slope_ = segment_data.end_tangent;

    // Update marker position.
    origin_ = segment_data.position;

    // If this is a 'move to' segment, save the point for use with 'close'.
    if (element.type == kPathElementMoveToPoint)
      subpath_start_ = element.points[0];
    else if (element.type == kPathElementCloseSubpath)
      subpath_start_ = FloatPoint();

    ++element_index_;
  }

  Vector<MarkerPosition>& positions_;
  unsigned element_index_;
  FloatPoint origin_;
  FloatPoint subpath_start_;
  FloatSize in_slope_;
  FloatSize out_slope_;
  bool auto_start_reverse_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_MARKER_DATA_H_
