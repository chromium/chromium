/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 *                     2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/path.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/geometry/skia_geometry_utils.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

bool PathQuadIntersection(const SkPath& path, const gfx::QuadF& quad) {
  const SkPath quad_path =
      SkPathBuilder()
          .moveTo(gfx::PointFToSkPoint(ClampNonFiniteToZero(quad.p1())))
          .lineTo(gfx::PointFToSkPoint(ClampNonFiniteToZero(quad.p2())))
          .lineTo(gfx::PointFToSkPoint(ClampNonFiniteToZero(quad.p3())))
          .lineTo(gfx::PointFToSkPoint(ClampNonFiniteToZero(quad.p4())))
          .close()
          .detach();
  const auto intersection = Op(path, quad_path, kIntersect_SkPathOp);

  return intersection && !intersection->isEmpty();
}

}  // namespace

Path::Path() = default;

Path::Path(const Path& other) = default;

Path::Path(const SkPath& other) : path_(other) {}

Path::~Path() = default;

Path& Path::operator=(const Path&) = default;

Path& Path::operator=(const SkPath& other) {
  path_ = other;
  return *this;
}

bool Path::operator==(const Path& other) const {
  return path_ == other.path_;
}

bool Path::Contains(const gfx::PointF& point) const {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
    return false;
  }
  return path_.contains(point.x(), point.y());
}

bool Path::Contains(const gfx::PointF& point, WindRule rule) const {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
    return false;
  }
  const float x = point.x();
  const float y = point.y();
  const SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);
  if (path_.getFillType() != fill_type) {
    SkPath tmp(path_);
    tmp.setFillType(fill_type);
    return tmp.contains(x, y);
  }
  return path_.contains(x, y);
}

bool Path::Intersects(const gfx::QuadF& quad) const {
  return PathQuadIntersection(path_, quad);
}

bool Path::Intersects(const gfx::QuadF& quad, WindRule rule) const {
  SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);
  if (path_.getFillType() != fill_type) {
    SkPath tmp(path_);
    tmp.setFillType(fill_type);
    return PathQuadIntersection(tmp, quad);
  }
  return PathQuadIntersection(path_, quad);
}

SkPath Path::StrokePath(const StrokeData& stroke_data,
                        const AffineTransform& transform) const {
  float stroke_precision = ClampTo<float>(
      sqrt(std::max(transform.XScaleSquared(), transform.YScaleSquared())));
  return StrokePath(stroke_data, stroke_precision);
}

SkPath Path::StrokePath(const StrokeData& stroke_data,
                        float stroke_precision) const {
  cc::PaintFlags flags;
  stroke_data.SetupPaint(&flags);

  SkPath stroke_path;
  flags.getFillPath(path_, &stroke_path, nullptr, stroke_precision);

  return stroke_path;
}

bool Path::StrokeContains(const gfx::PointF& point,
                          const StrokeData& stroke_data,
                          const AffineTransform& transform) const {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
    return false;
  }
  return StrokePath(stroke_data, transform).contains(point.x(), point.y());
}

gfx::RectF Path::TightBoundingRect() const {
  return gfx::SkRectToRectF(path_.computeTightBounds());
}

gfx::RectF Path::BoundingRect() const {
  return gfx::SkRectToRectF(path_.getBounds());
}

gfx::RectF Path::StrokeBoundingRect(const StrokeData& stroke_data) const {
  // Skia stroke resolution scale for reduced-precision requirements.
  constexpr float kStrokePrecision = 0.3f;
  return gfx::SkRectToRectF(
      StrokePath(stroke_data, kStrokePrecision).computeTightBounds());
}

static base::span<gfx::PointF> ConvertPathPoints(
    std::array<gfx::PointF, 3>& dst,
    base::span<const SkPoint> src) {
  for (size_t i = 0; i < src.size(); ++i) {
    dst[i] = gfx::SkPointToPointF(src[i]);
  }
  return base::span(dst).first(src.size());
}

void Path::Apply(void* info, PathApplierFunction function) const {
  SkPath::RawIter iter(path_);
  std::array<SkPoint, 4> pts;
  std::array<gfx::PointF, 3> path_points;
  PathElement path_element;

  for (;;) {
    switch (iter.next(pts.data())) {
      case SkPath::kMove_Verb:
        path_element.type = kPathElementMoveToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).first(1u));
        break;
      case SkPath::kLine_Verb:
        path_element.type = kPathElementAddLineToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).subspan<1, 1>());
        break;
      case SkPath::kQuad_Verb:
        path_element.type = kPathElementAddQuadCurveToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).subspan<1, 2>());
        break;
      case SkPath::kCubic_Verb:
        path_element.type = kPathElementAddCurveToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).subspan<1, 3>());
        break;
      case SkPath::kConic_Verb: {
        // Approximate with quads.  Use two for now, increase if more precision
        // is needed.
        const int kPow2 = 1;
        const unsigned kQuadCount = 1 << kPow2;
        std::array<SkPoint, 1 + 2 * kQuadCount> quads;
        SkPath::ConvertConicToQuads(pts[0], pts[1], pts[2], iter.conicWeight(),
                                    quads.data(), kPow2);

        path_element.type = kPathElementAddQuadCurveToPoint;
        for (unsigned i = 0; i < kQuadCount; ++i) {
          path_element.points = ConvertPathPoints(
              path_points, base::span(quads).subspan(1 + 2 * i, 2u));
          function(info, path_element);
        }
        continue;
      }
      case SkPath::kClose_Verb:
        path_element.type = kPathElementCloseSubpath;
        path_element.points = ConvertPathPoints(path_points, {});
        break;
      case SkPath::kDone_Verb:
        return;
    }
    function(info, path_element);
  }
}

float Path::length() const {
  float length = 0;
  SkPathMeasure measure(path_, false);

  do {
    length += measure.getLength();
  } while (measure.nextContour());

  return length;
}

gfx::PointF Path::PointAtLength(float length) const {
  return PointAndNormalAtLength(length).point;
}

static std::optional<PointAndTangent> CalculatePointAndNormalOnPath(
    SkPathMeasure& measure,
    float& contour_start,
    float length) {
  do {
    const float contour_end = contour_start + measure.getLength();
    if (length <= contour_end) {
      SkVector tangent;
      SkPoint position;

      const float pos_in_contour = length - contour_start;
      if (measure.getPosTan(pos_in_contour, &position, &tangent)) {
        PointAndTangent result;
        result.point = gfx::SkPointToPointF(position);
        result.tangent_in_degrees =
            Rad2deg(SkScalarATan2(tangent.fY, tangent.fX));
        return result;
      }
    }
    contour_start = contour_end;
  } while (measure.nextContour());
  return std::nullopt;
}

PointAndTangent Path::PointAndNormalAtLength(float length) const {
  SkPathMeasure measure(path_, false);
  float start = 0;
  if (std::optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
          measure, start, ClampNonFiniteToZero(length))) {
    return *result;
  }
  return {gfx::SkPointToPointF(path_.getPoint(0)), 0};
}

Path::PositionCalculator::PositionCalculator(const Path& path)
    : path_(path.GetSkPath()),
      path_measure_(path.GetSkPath(), false),
      accumulated_length_(0) {}

PointAndTangent Path::PositionCalculator::PointAndNormalAtLength(float length) {
  length = ClampNonFiniteToZero(length);
  if (length >= 0) {
    if (length < accumulated_length_) {
      // Reset path measurer to rewind (and restart from 0).
      path_measure_.setPath(&path_, false);
      accumulated_length_ = 0;
    }

    std::optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
        path_measure_, accumulated_length_, length);
    if (result) {
      return *result;
    }
  }
  return {gfx::SkPointToPointF(path_.getPoint(0)), 0};
}

bool Path::IsEmpty() const {
  return path_.isEmpty();
}

bool Path::IsClosed() const {
  return path_.isLastContourClosed();
}

bool Path::IsLine() const {
  return path_.isLine(nullptr);
}

Path Path::MakeRect(const gfx::RectF& rect) {
  return PathBuilder().AddRect(rect).Finalize();
}

Path Path::MakeRect(const gfx::PointF& origin,
                    const gfx::PointF& opposite_point) {
  return PathBuilder().AddRect(origin, opposite_point).Finalize();
}

Path Path::MakeContouredRect(const ContouredRect& crect) {
  return PathBuilder().AddContouredRect(crect).Finalize();
}

Path Path::MakeRoundedRect(const FloatRoundedRect& rrect) {
  return PathBuilder().AddRoundedRect(rrect).Finalize();
}

Path Path::MakeEllipse(const gfx::PointF& center,
                       float radius_x,
                       float radius_y) {
  return PathBuilder().AddEllipse(center, radius_x, radius_y).Finalize();
}

bool EllipseIsRenderable(float start_angle, float end_angle) {
  const float abs_sweep = std::abs(end_angle - start_angle);
  return (abs_sweep < kTwoPiFloat) ||
         WebCoreFloatNearlyEqual(abs_sweep, kTwoPiFloat);
}

}  // namespace blink
