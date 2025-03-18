// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path_builder.h"

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

gfx::Vector2dF SuperellipseAt(float s, float t) {
  float n = std::pow(2, s);
  float x = std::pow(t, 1 / n);
  float y = std::pow(1 - t, 1 / n);
  return gfx::Vector2dF(x, y);
}

// TODO(fserb) document this.
std::array<gfx::Vector2dF, 5> ApproximateSuperellipseAsBezierCurvePair(
    float curvature) {
  static constexpr std::array<double, 7> p = {
      1.2430920942724248, 2.010479023614843,  0.32922901179443753,
      0.2823023142212073, 1.3473704261055421, 2.9149468637949814,
      0.9106507102917086};

  DCHECK_GT(curvature, 0);
  const float s = std::log2(curvature);
  const float abs_s = std::abs(s);
  const float slope =
      p[0] + (p[6] - p[0]) * 0.5 * (1 + std::tanh(p[5] * (abs_s - p[1])));
  const float base = 1 / (1 + std::exp(-slope * (0 - p[1])));
  const float logistic = 1 / (1 + std::exp(-slope * (abs_s - p[1])));

  const float a = (logistic - base) / (1 - base);
  const float b = p[2] * std::exp(-p[3] * std::pow(abs_s, p[4]));

  gfx::Vector2dF P1(a, 1);
  gfx::Vector2dF P3 = SuperellipseAt(abs_s, 0.5);
  gfx::Vector2dF P5(1, a);

  if (s < 0) {
    P1 = gfx::Vector2dF(1 - P1.y(), 1 - P1.x());
    P3 = gfx::Vector2dF(1 - P3.y(), 1 - P3.x());
    P5 = gfx::Vector2dF(1 - P5.y(), 1 - P5.x());
  }

  gfx::Vector2dF P2(P3.x() - b, P3.y() + b);
  gfx::Vector2dF P4(P3.x() + b, P3.y() - b);
  return {P1, P2, P3, P4, P5};
}

enum class Corner { kTopLeft, kTopRight, kBottomRight, kBottomLeft };

void AddRoundedRectCornerShape(SkPath& path,
                               gfx::RectF corner_rect,
                               Corner corner,
                               float curvature) {
  gfx::PointF target_point;
  switch (corner) {
    case Corner::kTopLeft:
      target_point = corner_rect.top_right();
      break;
    case Corner::kTopRight:
      target_point = corner_rect.bottom_right();
      break;
    case Corner::kBottomRight:
      target_point = corner_rect.bottom_left();
      break;
    case Corner::kBottomLeft:
      target_point = corner_rect.origin();
      break;
  }
  if (curvature == ContouredRect::CornerCurvature::kBevel) {
    path.lineTo(gfx::PointFToSkPoint(target_point));
  } else if (curvature <= 0.001) {
    // Notch or very close to it, draw two lines.
    gfx::PointF control_point;
    switch (corner) {
      case Corner::kTopLeft:
        control_point = corner_rect.bottom_right();
        break;
      case Corner::kTopRight:
        control_point = corner_rect.bottom_left();
        break;
      case Corner::kBottomRight:
        control_point = corner_rect.origin();
        break;
      case Corner::kBottomLeft:
        control_point = corner_rect.top_right();
        break;
    }
    path.lineTo(gfx::PointFToSkPoint(control_point));
    path.lineTo(gfx::PointFToSkPoint(target_point));
  } else if (curvature == 2) {
    gfx::PointF control_point;
    switch (corner) {
      case Corner::kTopLeft:
        control_point = corner_rect.origin();
        break;
      case Corner::kTopRight:
        control_point = corner_rect.top_right();
        break;
      case Corner::kBottomRight:
        control_point = corner_rect.bottom_right();
        break;
      case Corner::kBottomLeft:
        control_point = corner_rect.bottom_left();
        break;
    }
    path.conicTo(gfx::PointFToSkPoint(control_point),
                 gfx::PointFToSkPoint(target_point), SK_ScalarRoot2Over2);
  } else {
    auto control_points = ApproximateSuperellipseAsBezierCurvePair(
        corner == Corner::kBottomRight || corner == Corner::kTopLeft
            ? 1 / curvature
            : curvature);
    gfx::PointF starting_point;
    switch (corner) {
      case Corner::kTopLeft:
        starting_point = corner_rect.bottom_left();
        break;
      case Corner::kTopRight:
        starting_point = corner_rect.origin();
        break;
      case Corner::kBottomRight:
        starting_point = corner_rect.top_right();
        break;
      case Corner::kBottomLeft:
        starting_point = corner_rect.bottom_right();
        break;
    }
    const gfx::Vector2dF target_vector = target_point - starting_point;
    std::array<SkPoint, 5> points;
    std::ranges::transform(
        control_points, points.begin(), [&](gfx::Vector2dF cv) {
          auto out_cv = gfx::ScaleVector2d(target_vector, cv.x(), 1 - cv.y());
          return gfx::PointFToSkPoint(starting_point + out_cv);
        });

    path.cubicTo(points.at(0), points.at(1), points.at(2));
    path.cubicTo(points.at(3), points.at(4),
                 gfx::PointFToSkPoint(target_point));
  }
}

}  // anonymous namespace

PathBuilder::PathBuilder() = default;
PathBuilder::~PathBuilder() = default;

PathBuilder::PathBuilder(const Path& path) : builder_(path.GetSkPath()) {}

void PathBuilder::Reset() {
  builder_.reset();
  current_path_.reset();
}

Path PathBuilder::Finalize() {
  Path path(std::move(builder_));

  Reset();

  return path;
}

gfx::RectF PathBuilder::BoundingRect() const {
  return gfx::SkRectToRectF(builder_.getBounds());
}

const Path& PathBuilder::CurrentPath() const {
  if (!current_path_) {
    current_path_.emplace(builder_);
  }

  return current_path_.value();
}

PathBuilder& PathBuilder::Close() {
  builder_.close();

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::MoveTo(const gfx::PointF& pt) {
  builder_.moveTo(gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::LineTo(const gfx::PointF& pt) {
  builder_.lineTo(gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::QuadTo(const gfx::PointF& ctrl,
                                 const gfx::PointF& pt) {
  builder_.quadTo(gfx::PointFToSkPoint(ctrl), gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::CubicTo(const gfx::PointF& ctrl1,
                                  const gfx::PointF& ctrl2,
                                  const gfx::PointF& pt) {
  builder_.cubicTo(gfx::PointFToSkPoint(ctrl1), gfx::PointFToSkPoint(ctrl2),
                   gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::ArcTo(const gfx::PointF& p,
                                float radius_x,
                                float radius_y,
                                float x_rotate,
                                bool large_arc,
                                bool sweep) {
  builder_.arcTo(radius_x, radius_y, x_rotate,
                 large_arc ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
                 sweep ? SkPathDirection::kCW : SkPathDirection::kCCW, p.x(),
                 p.y());

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRect(const gfx::RectF& rect) {
  // Start at upper-left, add clock-wise.
  builder_.addRect(gfx::RectFToSkRect(rect), SkPathDirection::kCW, 0);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRect(const gfx::PointF& origin,
                                  const gfx::PointF& opposite_point) {
  builder_.addRect(SkRect::MakeLTRB(origin.x(), origin.y(), opposite_point.x(),
                                    opposite_point.y()),
                   SkPathDirection::kCW, 0);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddPath(const Path& src,
                                  const AffineTransform& transform) {
  builder_.addPath(src.GetSkPath(), transform.ToSkMatrix());

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRoundedRect(const FloatRoundedRect& rect,
                                         bool clockwise) {
  if (rect.IsEmpty()) {
    return *this;
  }

  builder_.addRRect(SkRRect(rect),
                    clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW,
                    /* start at upper-left after corner radius */ 0);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddContouredRect(
    const ContouredRect& contoured_rect) {
  const FloatRoundedRect& rect = contoured_rect.AsRoundedRect();
  if (contoured_rect.HasRoundCurvature()) {
    return AddRoundedRect(rect);
  }

  const ContouredRect::CornerCurvature& curvature =
      contoured_rect.GetCornerCurvature();

  builder_.moveTo(gfx::PointFToSkPoint(rect.TopLeftCorner().top_right()));

  builder_.lineTo(gfx::PointFToSkPoint((rect.TopRightCorner().origin())));
  AddRoundedRectCornerShape(builder_, rect.TopRightCorner(), Corner::kTopRight,
                            curvature.TopRight());
  builder_.lineTo(gfx::PointFToSkPoint((rect.BottomRightCorner().top_right())));
  AddRoundedRectCornerShape(builder_, rect.BottomRightCorner(),
                            Corner::kBottomRight, curvature.BottomRight());
  builder_.lineTo(gfx::PointFToSkPoint(rect.BottomLeftCorner().bottom_right()));
  AddRoundedRectCornerShape(builder_, rect.BottomLeftCorner(),
                            Corner::kBottomLeft, curvature.BottomLeft());
  builder_.lineTo(gfx::PointFToSkPoint(rect.TopLeftCorner().bottom_left()));
  AddRoundedRectCornerShape(builder_, rect.TopLeftCorner(), Corner::kTopLeft,
                            curvature.TopLeft());
  return *this;
}

PathBuilder& PathBuilder::AddEllipse(const gfx::PointF& center,
                                     float radius_x,
                                     float radius_y) {
  // Start at 3 o'clock, add clock-wise.
  builder_.addOval(
      SkRect::MakeLTRB(center.x() - radius_x, center.y() - radius_y,
                       center.x() + radius_x, center.y() + radius_y),
      SkPathDirection::kCW, 1);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::SetWindRule(WindRule rule) {
  const SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);

  if (fill_type == builder_.getFillType()) {
    return *this;
  }

  builder_.setFillType(fill_type);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::Translate(const gfx::Vector2dF& offset) {
  builder_.offset(offset.x(), offset.y());

  current_path_.reset();
  return *this;
}

}  // namespace blink
