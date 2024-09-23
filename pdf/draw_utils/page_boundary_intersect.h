// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_PAGE_BOUNDARY_INTERSECT_H_
#define PDF_DRAW_UTILS_PAGE_BOUNDARY_INTERSECT_H_

#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chrome_pdf {

// Given:
// - `page_rect` must be non-empty.
// - `inside_point` must be inside of `page_rect`.
// - `outside_point` must be outside of `page_rect`.
//
// A straight line from `inside_point` to `outside_point` must intersect the
// boundary of `page_rect`. Return that intersection point.
gfx::PointF CalculatePageBoundaryIntersectPoint(
    const gfx::Rect& page_rect,
    const gfx::PointF& inside_point,
    const gfx::PointF& outside_point);

}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_PAGE_BOUNDARY_INTERSECT_H_
