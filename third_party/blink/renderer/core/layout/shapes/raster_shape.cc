/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/shapes/raster_shape.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class MarginIntervalGenerator {
 public:
  MarginIntervalGenerator(unsigned radius);
  void Set(int y, const IntShapeInterval&);
  IntShapeInterval IntervalAt(int y) const;

 private:
  Vector<int> x_intercepts_;
  int y_;
  int x1_;
  int x2_;
};

MarginIntervalGenerator::MarginIntervalGenerator(unsigned radius)
    : y_(0), x1_(0), x2_(0) {
  x_intercepts_.resize(radius + 1);
  unsigned radius_squared = radius * radius;
  for (unsigned y = 0; y <= radius; y++)
    x_intercepts_[y] = sqrt(static_cast<double>(radius_squared - y * y));
}

void MarginIntervalGenerator::Set(int y, const IntShapeInterval& interval) {
  DCHECK_GE(y, 0);
  DCHECK_GE(interval.X1(), 0);
  y_ = y;
  x1_ = interval.X1();
  x2_ = interval.X2();
}

IntShapeInterval MarginIntervalGenerator::IntervalAt(int y) const {
  unsigned x_intercepts_index = abs(y - y_);
  int dx = (x_intercepts_index >= x_intercepts_.size())
               ? 0
               : x_intercepts_[x_intercepts_index];
  return IntShapeInterval(x1_ - dx, x2_ + dx);
}

std::unique_ptr<RasterShapeIntervals>
RasterShapeIntervals::ComputeShapeMarginIntervals(int shape_margin) const {
  int margin_intervals_size = (Offset() > shape_margin)
                                  ? size()
                                  : size() - Offset() * 2 + shape_margin * 2;
  std::unique_ptr<RasterShapeIntervals> result =
      std::make_unique<RasterShapeIntervals>(margin_intervals_size,
                                             std::max(shape_margin, Offset()));
  MarginIntervalGenerator margin_interval_generator(shape_margin);

  for (int y = Bounds().y(); y < Bounds().bottom(); ++y) {
    const IntShapeInterval& interval_at_y = IntervalAt(y);
    if (interval_at_y.IsEmpty())
      continue;

    margin_interval_generator.Set(y, interval_at_y);
    int margin_y0 = std::max(MinY(), y - shape_margin);
    int margin_y1 = std::min(MaxY(), y + shape_margin + 1);

    for (int margin_y = y - 1; margin_y >= margin_y0; --margin_y) {
      if (margin_y > Bounds().y() &&
          IntervalAt(margin_y).Contains(interval_at_y))
        break;
      result->IntervalAt(margin_y).Unite(
          margin_interval_generator.IntervalAt(margin_y));
    }

    result->IntervalAt(y).Unite(margin_interval_generator.IntervalAt(y));

    for (int margin_y = y + 1; margin_y < margin_y1; ++margin_y) {
      if (margin_y < Bounds().bottom() &&
          IntervalAt(margin_y).Contains(interval_at_y))
        break;
      result->IntervalAt(margin_y).Unite(
          margin_interval_generator.IntervalAt(margin_y));
    }
  }

  result->InitializeBounds();
  return result;
}

void RasterShapeIntervals::InitializeBounds() {
  bounds_ = gfx::Rect();
  for (int y = MinY(); y < MaxY(); ++y) {
    const IntShapeInterval& interval_at_y = IntervalAt(y);
    if (interval_at_y.IsEmpty())
      continue;
    bounds_.Union(gfx::Rect(interval_at_y.X1(), y, interval_at_y.Width(), 1));
  }
}

void RasterShapeIntervals::BuildBoundsPath(Path& path) const {
  int max_y = Bounds().bottom();
  for (int y = Bounds().y(); y < max_y; y++) {
    if (IntervalAt(y).IsEmpty())
      continue;

    IntShapeInterval extent = IntervalAt(y);
    int end_y = y + 1;
    for (; end_y < max_y; end_y++) {
      if (IntervalAt(end_y).IsEmpty() || IntervalAt(end_y) != extent)
        break;
    }
    path.AddRect(gfx::PointF(extent.X1(), y), gfx::PointF(extent.X2(), end_y));
    y = end_y - 1;
  }
}

const RasterShapeIntervals& RasterShape::MarginIntervals() const {
  DCHECK_GE(ShapeMargin(), 0);
  if (!ShapeMargin())
    return *intervals_;

  int shape_margin_int = ClampTo<int>(ceil(ShapeMargin()), 0);
  int max_shape_margin_int =
      std::max(margin_rect_size_.width(), margin_rect_size_.height()) *
      sqrtf(2);
  if (!margin_intervals_)
    margin_intervals_ = intervals_->ComputeShapeMarginIntervals(
        std::min(shape_margin_int, max_shape_margin_int));

  return *margin_intervals_;
}

LineSegment RasterShape::GetExcludedInterval(LayoutUnit logical_top,
                                             LayoutUnit logical_height) const {
  const RasterShapeIntervals& intervals = MarginIntervals();
  if (intervals.IsEmpty())
    return LineSegment();

  int y1 = logical_top.ToInt();
  int y2 = (logical_top + logical_height).ToInt();
  DCHECK_GE(y2, y1);
  if (y2 < intervals.Bounds().y() || y1 >= intervals.Bounds().bottom())
    return LineSegment();

  y1 = std::max(y1, intervals.Bounds().y());
  y2 = std::min(y2, intervals.Bounds().bottom());
  IntShapeInterval excluded_interval;

  if (y1 == y2) {
    excluded_interval = intervals.IntervalAt(y1);
  } else {
    for (int y = y1; y < y2; y++)
      excluded_interval.Unite(intervals.IntervalAt(y));
  }

  // Note: |marginIntervals()| returns end-point exclusive
  // intervals. |excludedInterval.x2()| contains the left-most pixel
  // offset to the right of the calculated union.
  return LineSegment(excluded_interval.X1(), excluded_interval.X2());
}

}  // namespace blink
