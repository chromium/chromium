/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/shapes/polygon_shape.h"

#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static inline FloatSize InwardEdgeNormal(const FloatPolygonEdge& edge) {
  FloatSize edge_delta = edge.Vertex2() - edge.Vertex1();
  if (!edge_delta.Width())
    return FloatSize((edge_delta.Height() > 0 ? -1 : 1), 0);
  if (!edge_delta.Height())
    return FloatSize(0, (edge_delta.Width() > 0 ? 1 : -1));
  float edge_length = edge_delta.DiagonalLength();
  return FloatSize(-edge_delta.Height() / edge_length,
                   edge_delta.Width() / edge_length);
}

static inline FloatSize OutwardEdgeNormal(const FloatPolygonEdge& edge) {
  return -InwardEdgeNormal(edge);
}

static inline bool OverlapsYRange(const FloatRect& rect, float y1, float y2) {
  return !rect.IsEmpty() && y2 >= y1 && y2 >= rect.Y() && y1 <= rect.MaxY();
}

float OffsetPolygonEdge::XIntercept(float y) const {
  DCHECK_GE(y, MinY());
  DCHECK_LE(y, MaxY());

  if (Vertex1().Y() == Vertex2().Y() || Vertex1().X() == Vertex2().X())
    return MinX();
  if (y == MinY())
    return Vertex1().Y() < Vertex2().Y() ? Vertex1().X() : Vertex2().X();
  if (y == MaxY())
    return Vertex1().Y() > Vertex2().Y() ? Vertex1().X() : Vertex2().X();

  return Vertex1().X() +
         ((y - Vertex1().Y()) * (Vertex2().X() - Vertex1().X()) /
          (Vertex2().Y() - Vertex1().Y()));
}

FloatShapeInterval OffsetPolygonEdge::ClippedEdgeXRange(float y1,
                                                        float y2) const {
  if (!OverlapsYRange(y1, y2) || (y1 == MaxY() && MinY() <= y1) ||
      (y2 == MinY() && MaxY() >= y2))
    return FloatShapeInterval();

  if (IsWithinYRange(y1, y2))
    return FloatShapeInterval(MinX(), MaxX());

  // Clip the edge line segment to the vertical range y1,y2 and then return
  // the clipped line segment's horizontal range.

  FloatPoint min_y_vertex;
  FloatPoint max_y_vertex;
  if (Vertex1().Y() < Vertex2().Y()) {
    min_y_vertex = Vertex1();
    max_y_vertex = Vertex2();
  } else {
    min_y_vertex = Vertex2();
    max_y_vertex = Vertex1();
  }
  float x_for_y1 = (min_y_vertex.Y() < y1) ? XIntercept(y1) : min_y_vertex.X();
  float x_for_y2 = (max_y_vertex.Y() > y2) ? XIntercept(y2) : max_y_vertex.X();
  return FloatShapeInterval(std::min(x_for_y1, x_for_y2),
                            std::max(x_for_y1, x_for_y2));
}

static float CircleXIntercept(float y, float radius) {
  DCHECK_GT(radius, 0);
  return radius * sqrt(1 - (y * y) / (radius * radius));
}

static FloatShapeInterval ClippedCircleXRange(const FloatPoint& center,
                                              float radius,
                                              float y1,
                                              float y2) {
  if (y1 >= center.Y() + radius || y2 <= center.Y() - radius)
    return FloatShapeInterval();

  if (center.Y() >= y1 && center.Y() <= y2)
    return FloatShapeInterval(center.X() - radius, center.X() + radius);

  // Clip the circle to the vertical range y1,y2 and return the extent of the
  // clipped circle's projection on the X axis

  float xi = CircleXIntercept((y2 < center.Y() ? y2 : y1) - center.Y(), radius);
  return FloatShapeInterval(center.X() - xi, center.X() + xi);
}

LayoutRect PolygonShape::ShapeMarginLogicalBoundingBox() const {
  FloatRect box = polygon_.BoundingBox();
  box.Inflate(ShapeMargin());
  return LayoutRect(box);
}

LineSegment PolygonShape::GetExcludedInterval(LayoutUnit logical_top,
                                              LayoutUnit logical_height) const {
  float y1 = logical_top.ToFloat();
  float y2 = logical_top.ToFloat() + logical_height.ToFloat();

  if (polygon_.IsEmpty() ||
      !OverlapsYRange(polygon_.BoundingBox(), y1 - ShapeMargin(),
                      y2 + ShapeMargin()))
    return LineSegment();

  Vector<const FloatPolygonEdge*> overlapping_edges;
  if (!polygon_.OverlappingEdges(y1 - ShapeMargin(), y2 + ShapeMargin(),
                                 overlapping_edges))
    return LineSegment();

  FloatShapeInterval excluded_interval;
  for (unsigned i = 0; i < overlapping_edges.size(); i++) {
    const FloatPolygonEdge& edge = *(overlapping_edges[i]);
    if (edge.MaxY() == edge.MinY())
      continue;
    if (!ShapeMargin()) {
      excluded_interval.Unite(
          OffsetPolygonEdge(edge, FloatSize()).ClippedEdgeXRange(y1, y2));
    } else {
      excluded_interval.Unite(
          OffsetPolygonEdge(edge, OutwardEdgeNormal(edge) * ShapeMargin())
              .ClippedEdgeXRange(y1, y2));
      excluded_interval.Unite(
          OffsetPolygonEdge(edge, InwardEdgeNormal(edge) * ShapeMargin())
              .ClippedEdgeXRange(y1, y2));
      excluded_interval.Unite(
          ClippedCircleXRange(edge.Vertex1(), ShapeMargin(), y1, y2));
      excluded_interval.Unite(
          ClippedCircleXRange(edge.Vertex2(), ShapeMargin(), y1, y2));
    }
  }

  if (excluded_interval.IsEmpty())
    return LineSegment();

  return LineSegment(excluded_interval.X1(), excluded_interval.X2());
}

void PolygonShape::BuildDisplayPaths(DisplayPaths& paths) const {
  if (!polygon_.NumberOfVertices())
    return;
  paths.shape.MoveTo(polygon_.VertexAt(0));
  for (wtf_size_t i = 1; i < polygon_.NumberOfVertices(); ++i)
    paths.shape.AddLineTo(polygon_.VertexAt(i));
  paths.shape.CloseSubpath();
}

}  // namespace blink
