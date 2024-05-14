/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_path_query.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_consumer.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/platform/graphics/path_traversal_state.h"

namespace blink {

namespace {

class SVGPathTraversalState final : public SVGPathConsumer {
 public:
  SVGPathTraversalState(
      PathTraversalState::PathTraversalAction traversal_action,
      float desired_length = 0)
      : traversal_state_(traversal_action) {
    traversal_state_.desired_length_ = desired_length;
  }

  float TotalLength() const { return traversal_state_.total_length_; }
  gfx::PointF ComputedPoint() const { return traversal_state_.current_; }

  bool IsDone() const { return traversal_state_.success_; }

 private:
  void EmitSegment(const PathSegmentData&) override;

  PathTraversalState traversal_state_;
};

void SVGPathTraversalState::EmitSegment(const PathSegmentData& segment) {
  // Arcs normalize to one or more cubic bezier segments, so if we've already
  // processed enough (sub)segments we need not continue.
  if (traversal_state_.success_)
    return;
  switch (segment.command) {
    case kPathSegMoveToAbs:
      traversal_state_.total_length_ +=
          traversal_state_.MoveTo(segment.target_point);
      break;
    case kPathSegLineToAbs:
      traversal_state_.total_length_ +=
          traversal_state_.LineTo(segment.target_point);
      break;
    case kPathSegClosePath:
      traversal_state_.total_length_ += traversal_state_.CloseSubpath();
      break;
    case kPathSegCurveToCubicAbs:
      traversal_state_.total_length_ += traversal_state_.CubicBezierTo(
          segment.point1, segment.point2, segment.target_point);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  traversal_state_.ProcessSegment();
}

void ExecuteQuery(const SVGPathByteStream& path_byte_stream,
                  SVGPathTraversalState& traversal_state) {
  SVGPathByteStreamSource source(path_byte_stream);
  SVGPathNormalizer normalizer(&traversal_state);

  bool has_more_data = source.HasMoreData();
  while (has_more_data) {
    PathSegmentData segment = source.ParseSegment();
    DCHECK_NE(segment.command, kPathSegUnknown);

    normalizer.EmitSegment(segment);

    has_more_data = source.HasMoreData();
    if (traversal_state.IsDone())
      break;
  }
}

}  // namespace

SVGPathQuery::SVGPathQuery(const SVGPathByteStream& path_byte_stream)
    : path_byte_stream_(path_byte_stream) {}

float SVGPathQuery::GetTotalLength() const {
  SVGPathTraversalState traversal_state(
      PathTraversalState::kTraversalTotalLength);
  ExecuteQuery(path_byte_stream_, traversal_state);
  return traversal_state.TotalLength();
}

gfx::PointF SVGPathQuery::GetPointAtLength(float length) const {
  SVGPathTraversalState traversal_state(
      PathTraversalState::kTraversalPointAtLength, length);
  ExecuteQuery(path_byte_stream_, traversal_state);
  return traversal_state.ComputedPoint();
}

}  // namespace blink
