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

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

#include <algorithm>
#include <cmath>

#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

FloatRoundedRect::FloatRoundedRect(float x, float y, float width, float height)
    : rect_(x, y, width, height) {}

FloatRoundedRect::FloatRoundedRect(const gfx::RectF& rect, const Radii& radii)
    : rect_(rect), radii_(radii) {}

FloatRoundedRect::FloatRoundedRect(const gfx::Rect& rect, const Radii& radii)
    : rect_(rect), radii_(radii) {}

FloatRoundedRect::FloatRoundedRect(const gfx::RectF& rect,
                                   const gfx::SizeF& top_left,
                                   const gfx::SizeF& top_right,
                                   const gfx::SizeF& bottom_left,
                                   const gfx::SizeF& bottom_right)
    : rect_(rect), radii_(top_left, top_right, bottom_left, bottom_right) {}

FloatRoundedRect::FloatRoundedRect(const gfx::RRectF& r)
    : FloatRoundedRect(r.rect()) {
  gfx::Vector2dF top_left = r.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft);
  gfx::Vector2dF top_right = r.GetCornerRadii(gfx::RRectF::Corner::kUpperRight);
  gfx::Vector2dF bottom_left =
      r.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft);
  gfx::Vector2dF bottom_right =
      r.GetCornerRadii(gfx::RRectF::Corner::kLowerRight);
  SetRadii(Radii(gfx::SizeF(top_left.x(), top_left.y()),
                 gfx::SizeF(top_right.x(), top_right.y()),
                 gfx::SizeF(bottom_left.x(), bottom_left.y()),
                 gfx::SizeF(bottom_right.x(), bottom_right.y())));
}

void FloatRoundedRect::Radii::SetMinimumRadius(float minimum_radius) {
  top_left_.set_width(std::max(minimum_radius, top_left_.width()));
  top_left_.set_height(std::max(minimum_radius, top_left_.height()));
  top_right_.set_width(std::max(minimum_radius, top_right_.width()));
  top_right_.set_height(std::max(minimum_radius, top_right_.height()));
  bottom_left_.set_width(std::max(minimum_radius, bottom_left_.width()));
  bottom_left_.set_height(std::max(minimum_radius, bottom_left_.height()));
  bottom_right_.set_width(std::max(minimum_radius, bottom_right_.width()));
  bottom_right_.set_height(std::max(minimum_radius, bottom_right_.height()));
}

std::optional<float> FloatRoundedRect::Radii::UniformRadius() const {
  if (top_left_.width() == top_left_.height() && top_left_ == top_right_ &&
      top_left_ == bottom_left_ && top_left_ == bottom_right_) {
    return top_left_.width();
  }
  return std::nullopt;
}

void FloatRoundedRect::Radii::Scale(float factor) {
  if (factor == 1)
    return;

  // If either radius on a corner becomes zero, reset both radii on that corner.
  top_left_.Scale(factor);
  if (!top_left_.width() || !top_left_.height())
    top_left_ = gfx::SizeF();
  top_right_.Scale(factor);
  if (!top_right_.width() || !top_right_.height())
    top_right_ = gfx::SizeF();
  bottom_left_.Scale(factor);
  if (!bottom_left_.width() || !bottom_left_.height())
    bottom_left_ = gfx::SizeF();
  bottom_right_.Scale(factor);
  if (!bottom_right_.width() || !bottom_right_.height())
    bottom_right_ = gfx::SizeF();
}

void FloatRoundedRect::Radii::Outset(const gfx::OutsetsF& outsets) {
  if (top_left_.width() > 0)
    top_left_.set_width(top_left_.width() + outsets.left());
  if (top_left_.height() > 0)
    top_left_.set_height(top_left_.height() + outsets.top());
  if (top_right_.width() > 0)
    top_right_.set_width(top_right_.width() + outsets.right());
  if (top_right_.height() > 0)
    top_right_.set_height(top_right_.height() + outsets.top());
  if (bottom_left_.width() > 0)
    bottom_left_.set_width(bottom_left_.width() + outsets.left());
  if (bottom_left_.height() > 0)
    bottom_left_.set_height(bottom_left_.height() + outsets.bottom());
  if (bottom_right_.width() > 0)
    bottom_right_.set_width(bottom_right_.width() + outsets.right());
  if (bottom_right_.height() > 0)
    bottom_right_.set_height(bottom_right_.height() + outsets.bottom());
}

// From: https://drafts.csswg.org/css-backgrounds-3/#corner-shaping
// ... in order to create a sharper corner when the border radius is small (and
// thus ensure continuity between round and sharp corners), when the border
// radius is less than the margin, the margin is multiplied by the proportion
// 1 + (r-1)^3, where r is the ratio of the border radius to the margin, in
// calculating the corner radii of the margin box shape.
// And https://drafts.csswg.org/css-backgrounds-3/#shadow-shape:
// ... For example, if the border radius is 10px and the spread distance is
// 20px (r = .5), the corner radius of the shadow shape will be
// 10px + 20px × (1 + (.5 - 1)^3) = 27.5px rather than 30px. This adjustment
// is applied independently to the radii in each dimension.
static void OutsetCornerForMarginOrShadow(gfx::SizeF& corner,
                                          float width_outset,
                                          float height_outset) {
  if (corner.IsZero() || (width_outset == 0 && height_outset == 0))
    return;

  float width_factor = 1;
  if (corner.width() < std::abs(width_outset)) {
    width_factor = 1 + std::pow(corner.width() / std::abs(width_outset) - 1, 3);
  }

  float height_factor = 1;
  if (corner.height() == corner.width() && width_outset == height_outset) {
    height_factor = width_factor;
  } else if (corner.height() < std::abs(height_outset)) {
    height_factor =
        1 + std::pow(corner.height() / std::abs(height_outset) - 1, 3);
  }

  corner.set_width(std::max(corner.width() + width_factor * width_outset, 0.f));
  corner.set_height(
      std::max(corner.height() + height_factor * height_outset, 0.f));
}

void FloatRoundedRect::Radii::OutsetForMarginOrShadow(
    const gfx::OutsetsF& outsets) {
  OutsetCornerForMarginOrShadow(top_left_, outsets.left(), outsets.top());
  OutsetCornerForMarginOrShadow(top_right_, outsets.right(), outsets.top());
  OutsetCornerForMarginOrShadow(bottom_left_, outsets.left(), outsets.bottom());
  OutsetCornerForMarginOrShadow(bottom_right_, outsets.right(),
                                outsets.bottom());
}

void FloatRoundedRect::Radii::OutsetForShapeMargin(float outset) {
  // TODO(crbug.com/1309478): This isn't correct for non-circular
  // corners (that is, corners that have x and y radii that are not
  // equal).  But it's not clear to me if the correct result for that
  // case is even an ellipse.
  gfx::SizeF outset_size(outset, outset);
  top_left_ += outset_size;
  top_right_ += outset_size;
  bottom_left_ += outset_size;
  bottom_right_ += outset_size;
}

static inline float CornerRectIntercept(float y,
                                        const gfx::RectF& corner_rect) {
  DCHECK_GT(corner_rect.height(), 0);
  return corner_rect.width() *
         sqrt(1 - (y * y) / (corner_rect.height() * corner_rect.height()));
}

bool FloatRoundedRect::XInterceptsAtY(float y,
                                      float& min_x_intercept,
                                      float& max_x_intercept) const {
  if (y < Rect().y() || y > Rect().bottom())
    return false;

  if (!IsRounded()) {
    min_x_intercept = Rect().x();
    max_x_intercept = Rect().right();
    return true;
  }

  const gfx::RectF& top_left_rect = TopLeftCorner();
  const gfx::RectF& bottom_left_rect = BottomLeftCorner();

  if (!top_left_rect.IsEmpty() && y >= top_left_rect.y() &&
      y < top_left_rect.bottom()) {
    min_x_intercept =
        top_left_rect.right() -
        CornerRectIntercept(top_left_rect.bottom() - y, top_left_rect);
  } else if (!bottom_left_rect.IsEmpty() && y >= bottom_left_rect.y() &&
             y <= bottom_left_rect.bottom()) {
    min_x_intercept =
        bottom_left_rect.right() -
        CornerRectIntercept(y - bottom_left_rect.y(), bottom_left_rect);
  } else {
    min_x_intercept = rect_.x();
  }

  const gfx::RectF& top_right_rect = TopRightCorner();
  const gfx::RectF& bottom_right_rect = BottomRightCorner();

  if (!top_right_rect.IsEmpty() && y >= top_right_rect.y() &&
      y <= top_right_rect.bottom()) {
    max_x_intercept =
        top_right_rect.x() +
        CornerRectIntercept(top_right_rect.bottom() - y, top_right_rect);
  } else if (!bottom_right_rect.IsEmpty() && y >= bottom_right_rect.y() &&
             y <= bottom_right_rect.bottom()) {
    max_x_intercept =
        bottom_right_rect.x() +
        CornerRectIntercept(y - bottom_right_rect.y(), bottom_right_rect);
  } else {
    max_x_intercept = rect_.right();
  }

  return true;
}

void FloatRoundedRect::Outset(const gfx::OutsetsF& outsets) {
  rect_.Outset(outsets);
  radii_.Outset(outsets);
}

void FloatRoundedRect::OutsetForMarginOrShadow(const gfx::OutsetsF& outsets) {
  if (outsets.IsEmpty())
    return;
  rect_.Outset(outsets);
  radii_.OutsetForMarginOrShadow(outsets);
}

void FloatRoundedRect::OutsetForShapeMargin(float outset) {
  DCHECK_GE(outset, 0);
  if (outset == 0.f)
    return;
  rect_.Outset(outset);
  radii_.OutsetForShapeMargin(outset);
}

bool FloatRoundedRect::IntersectsQuad(const gfx::QuadF& quad) const {
  if (!quad.IntersectsRect(rect_))
    return false;

  const auto [quad_min, quad_max] = quad.Extents();

  // For each corner, first check the remaining (two) separating axes of the
  // rectangle that encloses the corner. The other (two) axes coincide with the
  // axes of `rect_`. If none of those are separating, proceed to call
  // IntersectsRectPartial to check the potential axes of `quad`.

  if (!radii_.TopLeft().IsEmpty()) {
    const gfx::RectF corner_rect(TopLeftCorner());
    if (quad_min.y() <= corner_rect.bottom() &&
        quad_min.x() <= corner_rect.right() &&
        quad.IntersectsRectPartial(corner_rect)) {
      if (!quad.IntersectsEllipse(corner_rect.bottom_right(),
                                  corner_rect.size())) {
        return false;
      }
    }
  }

  if (!radii_.TopRight().IsEmpty()) {
    const gfx::RectF corner_rect(TopRightCorner());
    if (quad_min.y() <= corner_rect.bottom() &&
        quad_max.x() >= corner_rect.x() &&
        quad.IntersectsRectPartial(corner_rect)) {
      if (!quad.IntersectsEllipse(corner_rect.bottom_left(),
                                  corner_rect.size())) {
        return false;
      }
    }
  }

  if (!radii_.BottomLeft().IsEmpty()) {
    const gfx::RectF corner_rect(BottomLeftCorner());
    if (quad_max.y() >= corner_rect.y() &&
        quad_min.x() <= corner_rect.right() &&
        quad.IntersectsRectPartial(corner_rect)) {
      if (!quad.IntersectsEllipse(corner_rect.top_right(),
                                  corner_rect.size())) {
        return false;
      }
    }
  }

  if (!radii_.BottomRight().IsEmpty()) {
    const gfx::RectF corner_rect(BottomRightCorner());
    if (quad_max.y() >= corner_rect.y() && quad_max.x() >= corner_rect.x() &&
        quad.IntersectsRectPartial(corner_rect)) {
      if (!quad.IntersectsEllipse(corner_rect.origin(), corner_rect.size())) {
        return false;
      }
    }
  }

  return true;
}

void FloatRoundedRect::ConstrainRadii() {
  float factor = 1;

  float horizontal_sum =
      std::max(radii_.TopLeft().width() + radii_.TopRight().width(),
               radii_.BottomLeft().width() + radii_.BottomRight().width());
  if (horizontal_sum > rect_.width())
    factor = std::min(rect_.width() / horizontal_sum, factor);

  float vertical_sum =
      std::max(radii_.TopLeft().height() + radii_.BottomLeft().height(),
               radii_.TopRight().height() + radii_.BottomRight().height());
  if (vertical_sum > rect_.height())
    factor = std::min(rect_.height() / vertical_sum, factor);

  DCHECK_LE(factor, 1);
  radii_.Scale(factor);
  DCHECK(IsRenderable());
}

bool FloatRoundedRect::IsRenderable() const {
  constexpr float kTolerance = 1.0001;
  return radii_.TopLeft().width() + radii_.TopRight().width() <=
             rect_.width() * kTolerance &&
         radii_.BottomLeft().width() + radii_.BottomRight().width() <=
             rect_.width() * kTolerance &&
         radii_.TopLeft().height() + radii_.BottomLeft().height() <=
             rect_.height() * kTolerance &&
         radii_.TopRight().height() + radii_.BottomRight().height() <=
             rect_.height() * kTolerance;
}

std::ostream& operator<<(std::ostream& ostream, const FloatRoundedRect& rect) {
  return ostream << rect.ToString();
}

std::ostream& operator<<(std::ostream& ostream,
                         const FloatRoundedRect::Radii& radii) {
  return ostream << radii.ToString();
}

String FloatRoundedRect::Radii::ToString() const {
  return String::Format(
      "tl:%s; tr:%s; bl:%s; br:%s", TopLeft().ToString().c_str(),
      TopRight().ToString().c_str(), BottomLeft().ToString().c_str(),
      BottomRight().ToString().c_str());
}

String FloatRoundedRect::ToString() const {
  if (Rect() == gfx::RectF(InfiniteIntRect())) {
    return "InfiniteIntRect";
  }
  if (GetRadii().IsZero())
    return String(Rect().ToString());
  return String(Rect().ToString()) + " radii:(" + GetRadii().ToString() + ")";
}

}  // namespace blink
