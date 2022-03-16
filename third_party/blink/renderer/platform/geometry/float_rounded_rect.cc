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
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
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

absl::optional<float> FloatRoundedRect::Radii::UniformRadius() const {
  if (top_left_.width() == top_left_.height() && top_left_ == top_right_ &&
      top_left_ == bottom_left_ && top_left_ == bottom_right_) {
    return top_left_.width();
  }
  return absl::nullopt;
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
static void OutsetCornerForMarginOrShadow(gfx::SizeF& corner, float outset) {
  if (corner.IsZero() || outset == 0)
    return;

  float width_factor = 1;
  if (corner.width() < outset)
    width_factor = 1 + pow(corner.width() / outset - 1, 3);

  float height_factor = 1;
  if (corner.height() == corner.width())
    height_factor = width_factor;
  else if (corner.height() < outset)
    height_factor = 1 + pow(corner.height() / outset - 1, 3);

  corner.set_width(corner.width() + width_factor * outset);
  corner.set_height(corner.height() + height_factor * outset);
}

void FloatRoundedRect::Radii::OutsetForMarginOrShadow(float outset) {
  OutsetCornerForMarginOrShadow(top_left_, outset);
  OutsetCornerForMarginOrShadow(top_right_, outset);
  OutsetCornerForMarginOrShadow(bottom_left_, outset);
  OutsetCornerForMarginOrShadow(bottom_right_, outset);
}

static inline float CornerRectIntercept(float y,
                                        const gfx::RectF& corner_rect) {
  DCHECK_GT(corner_rect.height(), 0);
  return corner_rect.width() *
         sqrt(1 - (y * y) / (corner_rect.height() * corner_rect.height()));
}

gfx::RectF FloatRoundedRect::RadiusCenterRect() const {
  gfx::InsetsF maximum_radius_insets(
      std::max(radii_.TopLeft().height(), radii_.TopRight().height()),
      std::max(radii_.TopRight().width(), radii_.BottomRight().width()),
      std::max(radii_.BottomLeft().height(), radii_.BottomRight().height()),
      std::max(radii_.TopLeft().width(), radii_.BottomLeft().width()));
  gfx::RectF center_rect(rect_);
  center_rect.Inset(maximum_radius_insets);
  return center_rect;
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

void FloatRoundedRect::OutsetForMarginOrShadow(float size) {
  if (size == 0.f)
    return;
  rect_.Outset(size);
  radii_.OutsetForMarginOrShadow(size);
}

void FloatRoundedRect::InflateWithRadii(int size) {
  gfx::RectF old = rect_;

  rect_.Outset(size);
  // Considering the inflation factor of shorter size to scale the radii seems
  // appropriate here
  float factor;
  if (rect_.width() < rect_.height())
    factor = old.width() ? (float)rect_.width() / old.width() : int(0);
  else
    factor = old.height() ? (float)rect_.height() / old.height() : int(0);

  radii_.Scale(factor);
}

bool FloatRoundedRect::IntersectsQuad(const gfx::QuadF& quad) const {
  if (!quad.IntersectsRect(rect_))
    return false;

  const gfx::SizeF& top_left = radii_.TopLeft();
  if (!top_left.IsEmpty()) {
    gfx::RectF rect(rect_.x(), rect_.y(), top_left.width(), top_left.height());
    if (quad.IntersectsRect(rect)) {
      gfx::PointF center(rect_.x() + top_left.width(),
                         rect_.y() + top_left.height());
      gfx::SizeF size(top_left.width(), top_left.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const gfx::SizeF& top_right = radii_.TopRight();
  if (!top_right.IsEmpty()) {
    gfx::RectF rect(rect_.right() - top_right.width(), rect_.y(),
                    top_right.width(), top_right.height());
    if (quad.IntersectsRect(rect)) {
      gfx::PointF center(rect_.right() - top_right.width(),
                         rect_.y() + top_right.height());
      gfx::SizeF size(top_right.width(), top_right.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const gfx::SizeF& bottom_left = radii_.BottomLeft();
  if (!bottom_left.IsEmpty()) {
    gfx::RectF rect(rect_.x(), rect_.bottom() - bottom_left.height(),
                    bottom_left.width(), bottom_left.height());
    if (quad.IntersectsRect(rect)) {
      gfx::PointF center(rect_.x() + bottom_left.width(),
                         rect_.bottom() - bottom_left.height());
      gfx::SizeF size(bottom_left.width(), bottom_left.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const gfx::SizeF& bottom_right = radii_.BottomRight();
  if (!bottom_right.IsEmpty()) {
    gfx::RectF rect(rect_.right() - bottom_right.width(),
                    rect_.bottom() - bottom_right.height(),
                    bottom_right.width(), bottom_right.height());
    if (quad.IntersectsRect(rect)) {
      gfx::PointF center(rect_.right() - bottom_right.width(),
                         rect_.bottom() - bottom_right.height());
      gfx::SizeF size(bottom_right.width(), bottom_right.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
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
  if (Rect() == gfx::RectF(LayoutRect::InfiniteIntRect()))
    return "InfiniteIntRect";
  if (GetRadii().IsZero())
    return String(Rect().ToString());
  return String(Rect().ToString()) + " radii:(" + GetRadii().ToString() + ")";
}

}  // namespace blink
