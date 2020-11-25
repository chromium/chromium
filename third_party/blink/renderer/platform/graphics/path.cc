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
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

namespace blink {

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

bool Path::Contains(const FloatPoint& point) const {
  if (!std::isfinite(point.X()) || !std::isfinite(point.Y()))
    return false;
  return path_.contains(SkScalar(point.X()), SkScalar(point.Y()));
}

bool Path::Contains(const FloatPoint& point, WindRule rule) const {
  if (!std::isfinite(point.X()) || !std::isfinite(point.Y()))
    return false;
  SkScalar x = point.X();
  SkScalar y = point.Y();
  SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);
  if (path_.getFillType() != fill_type) {
    SkPath tmp(path_);
    tmp.setFillType(fill_type);
    return tmp.contains(x, y);
  }
  return path_.contains(x, y);
}

SkPath Path::StrokePath(const StrokeData& stroke_data,
                        const AffineTransform& transform) const {
  float stroke_precision = clampTo<float>(
      sqrt(std::max(transform.XScaleSquared(), transform.YScaleSquared())));
  return StrokePath(stroke_data, stroke_precision);
}

SkPath Path::StrokePath(const StrokeData& stroke_data,
                        float stroke_precision) const {
  PaintFlags flags;
  stroke_data.SetupPaint(&flags);

  SkPath stroke_path;
  flags.getFillPath(path_, &stroke_path, nullptr, stroke_precision);

  return stroke_path;
}

bool Path::StrokeContains(const FloatPoint& point,
                          const StrokeData& stroke_data,
                          const AffineTransform& transform) const {
  if (!std::isfinite(point.X()) || !std::isfinite(point.Y()))
    return false;
  return StrokePath(stroke_data, transform)
      .contains(SkScalar(point.X()), SkScalar(point.Y()));
}

FloatRect Path::BoundingRect() const {
  return path_.computeTightBounds();
}

FloatRect Path::StrokeBoundingRect(const StrokeData& stroke_data) const {
  // Skia stroke resolution scale for reduced-precision requirements.
  constexpr float kStrokePrecision = 0.3f;
  return StrokePath(stroke_data, kStrokePrecision).computeTightBounds();
}

static FloatPoint* ConvertPathPoints(FloatPoint dst[],
                                     const SkPoint src[],
                                     int count) {
  for (int i = 0; i < count; i++) {
    dst[i].SetX(SkScalarToFloat(src[i].fX));
    dst[i].SetY(SkScalarToFloat(src[i].fY));
  }
  return dst;
}

void Path::Apply(void* info, PathApplierFunction function) const {
  SkPath::RawIter iter(path_);
  SkPoint pts[4];
  PathElement path_element;
  FloatPoint path_points[3];

  for (;;) {
    switch (iter.next(pts)) {
      case SkPath::kMove_Verb:
        path_element.type = kPathElementMoveToPoint;
        path_element.points = ConvertPathPoints(path_points, &pts[0], 1);
        break;
      case SkPath::kLine_Verb:
        path_element.type = kPathElementAddLineToPoint;
        path_element.points = ConvertPathPoints(path_points, &pts[1], 1);
        break;
      case SkPath::kQuad_Verb:
        path_element.type = kPathElementAddQuadCurveToPoint;
        path_element.points = ConvertPathPoints(path_points, &pts[1], 2);
        break;
      case SkPath::kCubic_Verb:
        path_element.type = kPathElementAddCurveToPoint;
        path_element.points = ConvertPathPoints(path_points, &pts[1], 3);
        break;
      case SkPath::kConic_Verb: {
        // Approximate with quads.  Use two for now, increase if more precision
        // is needed.
        const int kPow2 = 1;
        const unsigned kQuadCount = 1 << kPow2;
        SkPoint quads[1 + 2 * kQuadCount];
        SkPath::ConvertConicToQuads(pts[0], pts[1], pts[2], iter.conicWeight(),
                                    quads, kPow2);

        path_element.type = kPathElementAddQuadCurveToPoint;
        for (unsigned i = 0; i < kQuadCount; ++i) {
          path_element.points =
              ConvertPathPoints(path_points, &quads[1 + 2 * i], 2);
          function(info, &path_element);
        }
        continue;
      }
      case SkPath::kClose_Verb:
        path_element.type = kPathElementCloseSubpath;
        path_element.points = ConvertPathPoints(path_points, nullptr, 0);
        break;
      case SkPath::kDone_Verb:
        return;
    }
    function(info, &path_element);
  }
}

void Path::Transform(const AffineTransform& xform) {
  path_.transform(AffineTransformToSkMatrix(xform));
}

void Path::Transform(const TransformationMatrix& transformation_matrix) {
  path_.transform(TransformationMatrixToSkMatrix(transformation_matrix));
}

float Path::length() const {
  SkScalar length = 0;
  SkPathMeasure measure(path_, false);

  do {
    length += measure.getLength();
  } while (measure.nextContour());

  return SkScalarToFloat(length);
}

FloatPoint Path::PointAtLength(float length) const {
  return PointAndNormalAtLength(length).point;
}

static base::Optional<PointAndTangent> CalculatePointAndNormalOnPath(
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
        result.point = FloatPoint(position);
        result.tangent_in_degrees =
            rad2deg(SkScalarToFloat(SkScalarATan2(tangent.fY, tangent.fX)));
        return result;
      }
    }
    contour_start = contour_end;
  } while (measure.nextContour());
  return base::nullopt;
}

PointAndTangent Path::PointAndNormalAtLength(float length) const {
  SkPathMeasure measure(path_, false);
  SkScalar start = 0;
  if (base::Optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
          measure, start, WebCoreFloatToSkScalar(length)))
    return *result;
  return {FloatPoint(path_.getPoint(0)), 0};
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

    base::Optional<PointAndTangent> result = CalculatePointAndNormalOnPath(
        path_measure_, accumulated_length_, sk_length);
    if (result)
      return *result;
  }
  return {FloatPoint(path_.getPoint(0)), 0};
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

void Path::SetIsVolatile(bool is_volatile) {
  path_.setIsVolatile(is_volatile);
}

bool Path::HasCurrentPoint() const {
  return path_.getPoints(nullptr, 0);
}

FloatPoint Path::CurrentPoint() const {
  if (path_.countPoints() > 0) {
    SkPoint sk_result;
    path_.getLastPt(&sk_result);
    FloatPoint result;
    result.SetX(SkScalarToFloat(sk_result.fX));
    result.SetY(SkScalarToFloat(sk_result.fY));
    return result;
  }

  // FIXME: Why does this return quietNaN? Other ports return 0,0.
  float quiet_na_n = std::numeric_limits<float>::quiet_NaN();
  return FloatPoint(quiet_na_n, quiet_na_n);
}

void Path::SetWindRule(const WindRule rule) {
  path_.setFillType(WebCoreWindRuleToSkFillType(rule));
}

void Path::MoveTo(const FloatPoint& point) {
  path_.moveTo(FloatPointToSkPoint(point));
}

void Path::AddLineTo(const FloatPoint& point) {
  path_.lineTo(FloatPointToSkPoint(point));
}

void Path::AddQuadCurveTo(const FloatPoint& cp, const FloatPoint& ep) {
  path_.quadTo(FloatPointToSkPoint(cp), FloatPointToSkPoint(ep));
}

void Path::AddBezierCurveTo(const FloatPoint& p1,
                            const FloatPoint& p2,
                            const FloatPoint& ep) {
  path_.cubicTo(FloatPointToSkPoint(p1), FloatPointToSkPoint(p2),
                FloatPointToSkPoint(ep));
}

void Path::AddArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius) {
  path_.arcTo(FloatPointToSkPoint(p1), FloatPointToSkPoint(p2),
              WebCoreFloatToSkScalar(radius));
}

void Path::AddArcTo(const FloatPoint& p,
                    const FloatSize& r,
                    float x_rotate,
                    bool large_arc,
                    bool sweep) {
  path_.arcTo(WebCoreFloatToSkScalar(r.Width()),
              WebCoreFloatToSkScalar(r.Height()),
              WebCoreFloatToSkScalar(x_rotate),
              large_arc ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
              sweep ? SkPathDirection::kCW : SkPathDirection::kCCW,
              WebCoreFloatToSkScalar(p.X()), WebCoreFloatToSkScalar(p.Y()));
}

void Path::CloseSubpath() {
  path_.close();
}

void Path::AddEllipse(const FloatPoint& p,
                      float radius_x,
                      float radius_y,
                      float start_angle,
                      float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  SkScalar cx = WebCoreFloatToSkScalar(p.X());
  SkScalar cy = WebCoreFloatToSkScalar(p.Y());
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
    // SkPath::arcTo can't handle the sweepAngle that is equal to or greater
    // than 2Pi.
    path_.arcTo(oval, start_degrees, s180, false);
    path_.arcTo(oval, start_degrees + s180, s180, false);
    return;
  }
  if (SkScalarNearlyEqual(sweep_degrees, -s360)) {
    path_.arcTo(oval, start_degrees, -s180, false);
    path_.arcTo(oval, start_degrees - s180, -s180, false);
    return;
  }

  path_.arcTo(oval, start_degrees, sweep_degrees, false);
}

void Path::AddArc(const FloatPoint& p,
                  float radius,
                  float start_angle,
                  float end_angle) {
  AddEllipse(p, radius, radius, start_angle, end_angle);
}

void Path::AddRect(const FloatRect& rect) {
  // Start at upper-left, add clock-wise.
  path_.addRect(rect, SkPathDirection::kCW, 0);
}

void Path::AddEllipse(const FloatPoint& p,
                      float radius_x,
                      float radius_y,
                      float rotation,
                      float start_angle,
                      float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  if (!rotation) {
    AddEllipse(FloatPoint(p.X(), p.Y()), radius_x, radius_y, start_angle,
               end_angle);
    return;
  }

  // Add an arc after the relevant transform.
  AffineTransform ellipse_transform =
      AffineTransform::Translation(p.X(), p.Y()).RotateRadians(rotation);
  DCHECK(ellipse_transform.IsInvertible());
  AffineTransform inverse_ellipse_transform = ellipse_transform.Inverse();
  Transform(inverse_ellipse_transform);
  AddEllipse(FloatPoint::Zero(), radius_x, radius_y, start_angle, end_angle);
  Transform(ellipse_transform);
}

void Path::AddEllipse(const FloatRect& rect) {
  // Start at 3 o'clock, add clock-wise.
  path_.addOval(rect, SkPathDirection::kCW, 1);
}

void Path::AddRoundedRect(const FloatRoundedRect& r) {
  AddRoundedRect(r.Rect(), r.GetRadii().TopLeft(), r.GetRadii().TopRight(),
                 r.GetRadii().BottomLeft(), r.GetRadii().BottomRight());
}

void Path::AddRoundedRect(const FloatRect& rect,
                          const FloatSize& rounding_radii) {
  if (rect.IsEmpty())
    return;

  FloatSize radius(rounding_radii);
  FloatSize half_size(rect.Width() / 2, rect.Height() / 2);

  // Apply the SVG corner radius constraints, per the rect section of the SVG
  // shapes spec: if one of rx,ry is negative, then the other corner radius
  // value is used. If both values are negative then rx = ry = 0. If rx is
  // greater than half of the width of the rectangle then set rx to half of the
  // width; ry is handled similarly.

  if (radius.Width() < 0)
    radius.SetWidth((radius.Height() < 0) ? 0 : radius.Height());

  if (radius.Height() < 0)
    radius.SetHeight(radius.Width());

  if (radius.Width() > half_size.Width())
    radius.SetWidth(half_size.Width());

  if (radius.Height() > half_size.Height())
    radius.SetHeight(half_size.Height());

  AddPathForRoundedRect(rect, radius, radius, radius, radius);
}

void Path::AddRoundedRect(const FloatRect& rect,
                          const FloatSize& top_left_radius,
                          const FloatSize& top_right_radius,
                          const FloatSize& bottom_left_radius,
                          const FloatSize& bottom_right_radius) {
  if (rect.IsEmpty())
    return;

  if (rect.Width() < top_left_radius.Width() + top_right_radius.Width() ||
      rect.Width() < bottom_left_radius.Width() + bottom_right_radius.Width() ||
      rect.Height() < top_left_radius.Height() + bottom_left_radius.Height() ||
      rect.Height() <
          top_right_radius.Height() + bottom_right_radius.Height()) {
    // If all the radii cannot be accommodated, return a rect.
    // FIXME: Is this an error scenario, given that it appears the code in
    // FloatRoundedRect::constrainRadii() should be always called first? Should
    // we assert that this code is not reached? This fallback is very bad, since
    // it means that radii that are just barely too big due to rounding or
    // snapping will get completely ignored.
    AddRect(rect);
    return;
  }

  AddPathForRoundedRect(rect, top_left_radius, top_right_radius,
                        bottom_left_radius, bottom_right_radius);
}

void Path::AddPathForRoundedRect(const FloatRect& rect,
                                 const FloatSize& top_left_radius,
                                 const FloatSize& top_right_radius,
                                 const FloatSize& bottom_left_radius,
                                 const FloatSize& bottom_right_radius) {
  // Start at upper-left (after corner radii), add clock-wise.
  path_.addRRect(FloatRoundedRect(rect, top_left_radius, top_right_radius,
                                  bottom_left_radius, bottom_right_radius),
                 SkPathDirection::kCW, 0);
}

void Path::AddPath(const Path& src, const AffineTransform& transform) {
  path_.addPath(src.GetSkPath(), AffineTransformToSkMatrix(transform));
}

void Path::Translate(const FloatSize& size) {
  path_.offset(WebCoreFloatToSkScalar(size.Width()),
               WebCoreFloatToSkScalar(size.Height()));
}

bool Path::SubtractPath(const Path& other) {
  return Op(path_, other.path_, kDifference_SkPathOp, &path_);
}

bool Path::UnionPath(const Path& other) {
  return Op(path_, other.path_, kUnion_SkPathOp, &path_);
}

bool Path::IntersectPath(const Path& other) {
  return Op(path_, other.path_, kIntersect_SkPathOp, &path_);
}

bool EllipseIsRenderable(float start_angle, float end_angle) {
  return (std::abs(end_angle - start_angle) < kTwoPiFloat) ||
         WebCoreFloatNearlyEqual(std::abs(end_angle - start_angle),
                                 kTwoPiFloat);
}

}  // namespace blink
