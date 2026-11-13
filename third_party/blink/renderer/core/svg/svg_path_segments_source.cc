// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_segments_source.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_svg_path_segment.h"

namespace blink {

namespace {

// Expected value count per SVGPathSegType, indexed by enum value.
constexpr auto kPathSegmentArgumentCount = std::to_array<wtf_size_t>({
    0,  // kPathSegUnknown (unused)
    0,  // kPathSegClosePath
    2,  // kPathSegMoveToAbs
    2,  // kPathSegMoveToRel
    2,  // kPathSegLineToAbs
    2,  // kPathSegLineToRel
    6,  // kPathSegCurveToCubicAbs
    6,  // kPathSegCurveToCubicRel
    4,  // kPathSegCurveToQuadraticAbs
    4,  // kPathSegCurveToQuadraticRel
    7,  // kPathSegArcAbs
    7,  // kPathSegArcRel
    1,  // kPathSegLineToHorizontalAbs
    1,  // kPathSegLineToHorizontalRel
    1,  // kPathSegLineToVerticalAbs
    1,  // kPathSegLineToVerticalRel
    4,  // kPathSegCurveToCubicSmoothAbs
    4,  // kPathSegCurveToCubicSmoothRel
    2,  // kPathSegCurveToQuadraticSmoothAbs
    2,  // kPathSegCurveToQuadraticSmoothRel
});

// Single-character type string -> SVGPathSegType, or kPathSegUnknown.
SVGPathSegType ParseSegmentType(const String& type) {
  if (type.length() != 1) {
    return kPathSegUnknown;
  }
  return MapLetterToSegmentType(type[0]);
}

bool IsValidArcFlagValue(float value) {
  return value == 0.0f || value == 1.0f;
}

// Checks the type is known and the values are valid for that segment type.
bool ValidateSegment(SVGPathSegType type, const Vector<float>& values) {
  if (type == kPathSegUnknown) {
    return false;
  }
  if (values.size() != kPathSegmentArgumentCount[type]) {
    return false;
  }
  if (!std::ranges::all_of(values,
                           [](float value) { return std::isfinite(value); })) {
    return false;
  }
  if (type == kPathSegArcAbs || type == kPathSegArcRel) {
    return IsValidArcFlagValue(values[3]) && IsValidArcFlagValue(values[4]);
  }
  return true;
}

// Fills a PathSegmentData from a pre-validated type/values pair.
PathSegmentData BuildSegment(SVGPathSegType type, const Vector<float>& values) {
  PathSegmentData segment;
  segment.command = type;
  switch (type) {
    case kPathSegClosePath:
      break;
    case kPathSegMoveToAbs:
    case kPathSegMoveToRel:
    case kPathSegLineToAbs:
    case kPathSegLineToRel:
    case kPathSegCurveToQuadraticSmoothAbs:
    case kPathSegCurveToQuadraticSmoothRel:
      segment.target_point = {values[0], values[1]};
      break;
    case kPathSegLineToHorizontalAbs:
    case kPathSegLineToHorizontalRel:
      segment.target_point.set_x(values[0]);
      break;
    case kPathSegLineToVerticalAbs:
    case kPathSegLineToVerticalRel:
      segment.target_point.set_y(values[0]);
      break;
    case kPathSegCurveToCubicAbs:
    case kPathSegCurveToCubicRel:
      segment.point1 = {values[0], values[1]};
      segment.point2 = {values[2], values[3]};
      segment.target_point = {values[4], values[5]};
      break;
    case kPathSegCurveToCubicSmoothAbs:
    case kPathSegCurveToCubicSmoothRel:
      segment.point2 = {values[0], values[1]};
      segment.target_point = {values[2], values[3]};
      break;
    case kPathSegCurveToQuadraticAbs:
    case kPathSegCurveToQuadraticRel:
      segment.point1 = {values[0], values[1]};
      segment.target_point = {values[2], values[3]};
      break;
    case kPathSegArcAbs:
    case kPathSegArcRel:
      segment.point1 = {values[0], values[1]};
      segment.point2.set_x(values[2]);
      segment.arc_large = values[3] == 1.0f;
      segment.arc_sweep = values[4] == 1.0f;
      segment.target_point = {values[5], values[6]};
      break;
    case kPathSegUnknown:
      NOTREACHED();
  }
  return segment;
}

}  // namespace

PathSegmentData SVGPathSegmentsSource::ParseSegment() {
  const SVGPathSegment* segment = segments_[index_];
  const bool is_first = index_ == 0;
  ++index_;

  const SVGPathSegType type = ParseSegmentType(segment->type());
  const Vector<float>& values = segment->values();

  // SVG Paths 9.7: a path must begin with a moveto.
  if (is_first && type != kPathSegMoveToAbs && type != kPathSegMoveToRel) {
    return PathSegmentData();
  }
  if (!ValidateSegment(type, values)) {
    return PathSegmentData();
  }
  return BuildSegment(type, values);
}

}  // namespace blink
