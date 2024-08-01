// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/page_boundary_intersect.h"

#include "base/check.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

// gfx::Rect considers points on its bottom and right border to be not contained
// within the rect. So to make sure CalculatePageBoundaryIntersectPoint()
// returns points within the page, use this constant to move them inwards a bit.
constexpr float kBoundaryEpsilon = 0.0001f;

}  // namespace

gfx::PointF CalculatePageBoundaryIntersectPoint(
    const gfx::Rect& page_rect,
    const gfx::PointF& inside_point,
    const gfx::PointF& outside_point) {
  const gfx::RectF rect(page_rect);
  CHECK(!rect.IsEmpty());
  CHECK(rect.Contains(inside_point));
  CHECK(!rect.Contains(outside_point));

  const float x_diff = outside_point.x() - inside_point.x();
  const float y_diff = outside_point.y() - inside_point.y();
  // Handle the special case where calculating the slope would divide by 0.
  if (x_diff == 0) {
    if (y_diff > 0) {
      return {inside_point.x(), rect.bottom() - kBoundaryEpsilon};
    }
    return {inside_point.x(), rect.y()};
  }

  // Handle the special case where dividing by the slope would be dividing by 0.
  if (y_diff == 0) {
    if (x_diff > 0) {
      return {rect.right() - kBoundaryEpsilon, inside_point.y()};
    }
    return {rect.x(), inside_point.y()};
  }

  // For all other cases, calculate where the line between `inside_point` and
  // `outside_point` would intersect `rect` on its horizontal and vertical
  // boundaries, assuming the line going towards `outside_point` is infinitely
  // long in that direction, and the boundary lines also extends infinitely.
  const float slope = y_diff / x_diff;
  gfx::PointF left_or_right_boundary_intersection_point;
  if (x_diff > 0) {
    float y_distance = (rect.right() - inside_point.x()) * slope;
    left_or_right_boundary_intersection_point = {
        rect.right() - kBoundaryEpsilon, inside_point.y() + y_distance};
  } else {
    float y_distance = (inside_point.x() - rect.x()) * slope;
    left_or_right_boundary_intersection_point = {rect.x(),
                                                 inside_point.y() - y_distance};
  }

  gfx::PointF top_or_bottom_boundary_intersection_point;
  if (y_diff > 0) {
    float x_distance = (rect.bottom() - inside_point.y()) / slope;
    top_or_bottom_boundary_intersection_point = {
        inside_point.x() + x_distance, rect.bottom() - kBoundaryEpsilon};
  } else {
    float x_distance = (inside_point.y() - rect.y()) / slope;
    top_or_bottom_boundary_intersection_point = {inside_point.x() - x_distance,
                                                 rect.y()};
  }

  // Then return the result closest to `inside_point`.
  float left_or_right_point_length =
      (left_or_right_boundary_intersection_point - inside_point).Length();
  float top_or_bottom_point_length =
      (top_or_bottom_boundary_intersection_point - inside_point).Length();
  return left_or_right_point_length < top_or_bottom_point_length
             ? left_or_right_boundary_intersection_point
             : top_or_bottom_boundary_intersection_point;
}

}  // namespace chrome_pdf
