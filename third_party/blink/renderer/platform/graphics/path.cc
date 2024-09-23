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

#include "third_party/blink/renderer/platform/graphics/path.h"

#include <math.h>
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

bool PathQuadIntersection(const SkPath& path, const gfx::QuadF& quad) {
  SkPath quad_path, intersection;
  quad_path.moveTo(FloatPointToSkPoint(quad.p1()))
      .lineTo(FloatPointToSkPoint(quad.p2()))
      .lineTo(FloatPointToSkPoint(quad.p3()))
      .lineTo(FloatPointToSkPoint(quad.p4()))
      .close();
  if (!Op(path, quad_path, kIntersect_SkPathOp, &intersection)) {
    return false;
  }
  return !intersection.isEmpty();
}

}  // namespace

Path::Path() : path_() {}

Path::Path(const Path& other) : path_(other.path_) {}

Path::Path(const SkPath& other) : path_(other) {}

Path::~Path() = default;

Path& Path::operator=(const Path& other) {
  path_ = other.path_;
  return *this;
}

Path& Path::operator=(const SkPath& other) {
  path_ = other;
  return *this;
}

bool Path::operator==(const Path& other) const {
  return path_ == other.path_;
}

bool Path::Contains(const gfx::PointF& point) const {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
    return false;
  return path_.contains(SkScalar(point.x()), SkScalar(point.y()));
}

bool Path::Contains(const gfx::PointF& point, WindRule rule) const {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
    return false;
  SkScalar x = point.x();
  SkScalar y = point.y();
  SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);
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
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
    return false;
  return StrokePath(stroke_data, transform)
      .contains(SkScalar(point.x()), SkScalar(point.y()));
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
    const SkPoint& src_point = src[i];
    dst[i].set_x(SkScalarToFloat(src_point.fX));
    dst[i].set_y(SkScalarToFloat(src_point.fY));
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
            ConvertPathPoints(path_points, base::span(pts).subspan(1, 1));
        break;
      case SkPath::kQuad_Verb:
        path_element.type = kPathElementAddQuadCurveToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).subspan(1, 2));
        break;
      case SkPath::kCubic_Verb:
        path_element.type = kPathElementAddCurveToPoint;
        path_element.points =
            ConvertPathPoints(path_points, base::span(pts).subspan(1, 3));
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
              path_points, base::span(quads).subspan(1 + 2 * i, 2));
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

Path& Path::Transform(const AffineTransform& xform) {
  path_.transform(AffineTransformToSkMatrix(xform));
  return *this;
}

Path& Path::Transform(const gfx::Transform& transform) {
  path_.transform(gfx::TransformToFlattenedSkMatrix(transform));
  return *this;
}

float Path::length() const {
  SkScalar length = 0;
  SkPathMeasure measure(path_, false);

  do {
    length += measure.getLength();
  } while (measure.nextContour());

  return SkScalarToFloat(length);
}

gfx::PointF Path::PointAtLength(float length) const {
  return PointAndNormalAtLength(length).point;
}

static std::optional<PointAndTangent> CalculatePointAndNormalOnPath(
    SkPathMeasure& measure,
    SkScalar& contour_start,
    SkScalar length) {
  do {
    SkScalar contour_end = contour_start + measure.getLength();
    if (length <= contour_end) {
      SkVector tangent;
      SkPoint position;

      SkScalar pos_in_contour = length - contour_start;
      if (measure.getPosTan(pos_in_contour, &position, &tangent)) {
        PointAndTangent result;
        result.point = gfx::SkPointToPointF(position);
        result.tangent_in_degrees =
            Rad2deg(SkScalarToFloat(SkScalarATan2(tangent.fY, tangent.fX)));
        return result;
      }
    }
    contour_start = contour_end;
  } while (measure.nextContour());
  return std::nullopt;
}

PointAndTangent Path::PointAndNormalAtLength(float length) const {
  SkPathMeasure measure(path_, false);
  SkScalar start = 0;
  if (std::optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
          measure, start, WebCoreFloatToSkScalar(length))) {
    return *result;
  }
  return {gfx::SkPointToPointF(path_.getPoint(0)), 0};
}

Path::PositionCalculator::PositionCalculator(const Path& path)
    : path_(path.GetSkPath()),
      path_measure_(path.GetSkPath(), false),
      accumulated_length_(0) {}

PointAndTangent Path::PositionCalculator::PointAndNormalAtLength(float length) {
  SkScalar sk_length = WebCoreFloatToSkScalar(length);
  if (sk_length >= 0) {
    if (sk_length < accumulated_length_) {
      // Reset path measurer to rewind (and restart from 0).
      path_measure_.setPath(&path_, false);
      accumulated_length_ = 0;
    }

    std::optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
        path_measure_, accumulated_length_, sk_length);
    if (result)
      return *result;
  }
  return {gfx::SkPointToPointF(path_.getPoint(0)), 0};
}

void Path::Clear() {
  path_.reset();
}

bool Path::IsEmpty() const {
  return path_.isEmpty();
}

bool Path::IsClosed() const {
  return path_.isLastContourClosed();
}

bool Path::IsLine() const {
  SkPoint dummy_line[2];
  return path_.isLine(dummy_line);
}

void Path::SetIsVolatile(bool is_volatile) {
  path_.setIsVolatile(is_volatile);
}

bool Path::HasCurrentPoint() const {
  return path_.getPoints(nullptr, 0);
}

gfx::PointF Path::CurrentPoint() const {
  if (path_.countPoints() > 0) {
    SkPoint sk_result;
    path_.getLastPt(&sk_result);
    gfx::PointF result;
    result.set_x(SkScalarToFloat(sk_result.fX));
    result.set_y(SkScalarToFloat(sk_result.fY));
    return result;
  }

  // FIXME: Why does this return quietNaN? Other ports return 0,0.
  float quiet_na_n = std::numeric_limits<float>::quiet_NaN();
  return gfx::PointF(quiet_na_n, quiet_na_n);
}

void Path::SetWindRule(const WindRule rule) {
  path_.setFillType(WebCoreWindRuleToSkFillType(rule));
}

void Path::MoveTo(const gfx::PointF& point) {
  path_.moveTo(gfx::PointFToSkPoint(point));
}

void Path::AddLineTo(const gfx::PointF& point) {
  path_.lineTo(gfx::PointFToSkPoint(point));
}

void Path::AddQuadCurveTo(const gfx::PointF& cp, const gfx::PointF& ep) {
  path_.quadTo(gfx::PointFToSkPoint(cp), gfx::PointFToSkPoint(ep));
}

void Path::AddBezierCurveTo(const gfx::PointF& p1,
                            const gfx::PointF& p2,
                            const gfx::PointF& ep) {
  path_.cubicTo(gfx::PointFToSkPoint(p1), gfx::PointFToSkPoint(p2),
                gfx::PointFToSkPoint(ep));
}

void Path::AddArcTo(const gfx::PointF& p1,
                    const gfx::PointF& p2,
                    float radius) {
  path_.arcTo(gfx::PointFToSkPoint(p1), gfx::PointFToSkPoint(p2),
              WebCoreFloatToSkScalar(radius));
}

void Path::AddArcTo(const gfx::PointF& p,
                    float radius_x,
                    float radius_y,
                    float x_rotate,
                    bool large_arc,
                    bool sweep) {
  path_.arcTo(WebCoreFloatToSkScalar(radius_x),
              WebCoreFloatToSkScalar(radius_y),
              WebCoreFloatToSkScalar(x_rotate),
              large_arc ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
              sweep ? SkPathDirection::kCW : SkPathDirection::kCCW,
              WebCoreFloatToSkScalar(p.x()), WebCoreFloatToSkScalar(p.y()));
}

void Path::CloseSubpath() {
  path_.close();
}

void Path::AddEllipse(const gfx::PointF& p,
                      float radius_x,
                      float radius_y,
                      float start_angle,
                      float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  SkScalar cx = WebCoreFloatToSkScalar(p.x());
  SkScalar cy = WebCoreFloatToSkScalar(p.y());
  SkScalar radius_x_scalar = WebCoreFloatToSkScalar(radius_x);
  SkScalar radius_y_scalar = WebCoreFloatToSkScalar(radius_y);

  SkRect oval;
  oval.setLTRB(cx - radius_x_scalar, cy - radius_y_scalar, cx + radius_x_scalar,
               cy + radius_y_scalar);

  float sweep = end_angle - start_angle;
  SkScalar start_degrees = WebCoreFloatToSkScalar(start_angle * 180 / kPiFloat);
  SkScalar sweep_degrees = WebCoreFloatToSkScalar(sweep * 180 / kPiFloat);
  SkScalar s360 = SkIntToScalar(360);

  // We can't use SkPath::addOval(), because addOval() makes a new sub-path.
  // addOval() calls moveTo() and close() internally.

  // Use s180, not s360, because SkPath::arcTo(oval, angle, s360, false) draws
  // nothing.
  SkScalar s180 = SkIntToScalar(180);
  if (SkScalarNearlyEqual(sweep_degrees, s360)) {
    // incReserve() results in a single allocation instead of multiple as is
    // done by multiple calls to arcTo().
    path_.incReserve(10, 5, 4);
    // SkPath::arcTo can't handle the sweepAngle that is equal to or greater
    // than 2Pi.
    path_.arcTo(oval, start_degrees, s180, false);
    path_.arcTo(oval, start_degrees + s180, s180, false);
    return;
  }
  if (SkScalarNearlyEqual(sweep_degrees, -s360)) {
    // incReserve() results in a single allocation instead of multiple as is
    // done by multiple calls to arcTo().
    path_.incReserve(10, 5, 4);
    path_.arcTo(oval, start_degrees, -s180, false);
    path_.arcTo(oval, start_degrees - s180, -s180, false);
    return;
  }

  path_.arcTo(oval, start_degrees, sweep_degrees, false);
}

void Path::AddArc(const gfx::PointF& p,
                  float radius,
                  float start_angle,
                  float end_angle) {
  AddEllipse(p, radius, radius, start_angle, end_angle);
}

void Path::AddRect(const gfx::RectF& rect) {
  // Start at upper-left, add clock-wise.
  path_.addRect(gfx::RectFToSkRect(rect), SkPathDirection::kCW, 0);
}

void Path::AddRect(const gfx::PointF& origin,
                   const gfx::PointF& opposite_point) {
  path_.addRect(SkRect::MakeLTRB(origin.x(), origin.y(), opposite_point.x(),
                                 opposite_point.y()),
                SkPathDirection::kCW, 0);
}

void Path::AddEllipse(const gfx::PointF& p,
                      float radius_x,
                      float radius_y,
                      float rotation,
                      float start_angle,
                      float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  if (!rotation) {
    AddEllipse(p, radius_x, radius_y, start_angle, end_angle);
    return;
  }

  // Add an arc after the relevant transform.
  AffineTransform ellipse_transform =
      AffineTransform::Translation(p.x(), p.y()).RotateRadians(rotation);
  DCHECK(ellipse_transform.IsInvertible());
  AffineTransform inverse_ellipse_transform = ellipse_transform.Inverse();
  Transform(inverse_ellipse_transform);
  AddEllipse(gfx::PointF(), radius_x, radius_y, start_angle, end_angle);
  Transform(ellipse_transform);
}

void Path::AddEllipse(const gfx::PointF& center,
                      float radius_x,
                      float radius_y) {
  // Start at 3 o'clock, add clock-wise.
  path_.addOval(SkRect::MakeLTRB(center.x() - radius_x, center.y() - radius_y,
                                 center.x() + radius_x, center.y() + radius_y),
                SkPathDirection::kCW, 1);
}

void Path::AddRoundedRect(const FloatRoundedRect& rect, bool clockwise) {
  if (rect.IsEmpty())
    return;

  path_.addRRect(SkRRect(rect),
                 clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW,
                 /* start at upper-left after corner radius */ 0);
}

void Path::AddPath(const Path& src, const AffineTransform& transform) {
  path_.addPath(src.GetSkPath(), AffineTransformToSkMatrix(transform));
}

void Path::Translate(const gfx::Vector2dF& offset) {
  path_.offset(WebCoreFloatToSkScalar(offset.x()),
               WebCoreFloatToSkScalar(offset.y()));
}

bool Path::SubtractPath(const Path& other) {
  return Op(path_, other.path_, kDifference_SkPathOp, &path_);
}

bool Path::UnionPath(const Path& other) {
  return Op(path_, other.path_, kUnion_SkPathOp, &path_);
}

bool EllipseIsRenderable(float start_angle, float end_angle) {
  return (std::abs(end_angle - start_angle) < kTwoPiFloat) ||
         WebCoreFloatNearlyEqual(std::abs(end_angle - start_angle),
                                 kTwoPiFloat);
}

}  // namespace blink
