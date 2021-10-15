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
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FloatRoundedRect::FloatRoundedRect(float x, float y, float width, float height)
    : rect_(x, y, width, height) {}

FloatRoundedRect::FloatRoundedRect(const FloatRect& rect, const Radii& radii)
    : rect_(rect), radii_(radii) {}

FloatRoundedRect::FloatRoundedRect(const IntRect& rect, const Radii& radii)
    : rect_(FloatRect(rect)), radii_(radii) {}

FloatRoundedRect::FloatRoundedRect(const FloatRect& rect,
                                   const FloatSize& top_left,
                                   const FloatSize& top_right,
                                   const FloatSize& bottom_left,
                                   const FloatSize& bottom_right)
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
    top_left_ = FloatSize();
  top_right_.Scale(factor);
  if (!top_right_.width() || !top_right_.height())
    top_right_ = FloatSize();
  bottom_left_.Scale(factor);
  if (!bottom_left_.width() || !bottom_left_.height())
    bottom_left_ = FloatSize();
  bottom_right_.Scale(factor);
  if (!bottom_right_.width() || !bottom_right_.height())
    bottom_right_ = FloatSize();
}

void FloatRoundedRect::Radii::ScaleAndFloor(float factor) {
  if (factor == 1)
    return;

  // If either radius on a corner becomes zero, reset both radii on that corner.
  top_left_.ScaleAndFloor(factor);
  if (!top_left_.width() || !top_left_.height())
    top_left_ = FloatSize();
  top_right_.ScaleAndFloor(factor);
  if (!top_right_.width() || !top_right_.height())
    top_right_ = FloatSize();
  bottom_left_.ScaleAndFloor(factor);
  if (!bottom_left_.width() || !bottom_left_.height())
    bottom_left_ = FloatSize();
  bottom_right_.ScaleAndFloor(factor);
  if (!bottom_right_.width() || !bottom_right_.height())
    bottom_right_ = FloatSize();
}

void FloatRoundedRect::Radii::Shrink(float top_width,
                                     float bottom_width,
                                     float left_width,
                                     float right_width) {
  DCHECK_GE(top_width, 0);
  DCHECK_GE(bottom_width, 0);
  DCHECK_GE(left_width, 0);
  DCHECK_GE(right_width, 0);

  top_left_.set_width(std::max<float>(0, top_left_.width() - left_width));
  top_left_.set_height(std::max<float>(0, top_left_.height() - top_width));

  top_right_.set_width(std::max<float>(0, top_right_.width() - right_width));
  top_right_.set_height(std::max<float>(0, top_right_.height() - top_width));

  bottom_left_.set_width(std::max<float>(0, bottom_left_.width() - left_width));
  bottom_left_.set_height(
      std::max<float>(0, bottom_left_.height() - bottom_width));

  bottom_right_.set_width(
      std::max<float>(0, bottom_right_.width() - right_width));
  bottom_right_.set_height(
      std::max<float>(0, bottom_right_.height() - bottom_width));
}

void FloatRoundedRect::Radii::Expand(float top_width,
                                     float bottom_width,
                                     float left_width,
                                     float right_width) {
  DCHECK_GE(top_width, 0);
  DCHECK_GE(bottom_width, 0);
  DCHECK_GE(left_width, 0);
  DCHECK_GE(right_width, 0);
  if (top_left_.width() > 0 && top_left_.height() > 0) {
    top_left_.set_width(top_left_.width() + left_width);
    top_left_.set_height(top_left_.height() + top_width);
  }
  if (top_right_.width() > 0 && top_right_.height() > 0) {
    top_right_.set_width(top_right_.width() + right_width);
    top_right_.set_height(top_right_.height() + top_width);
  }
  if (bottom_left_.width() > 0 && bottom_left_.height() > 0) {
    bottom_left_.set_width(bottom_left_.width() + left_width);
    bottom_left_.set_height(bottom_left_.height() + bottom_width);
  }
  if (bottom_right_.width() > 0 && bottom_right_.height() > 0) {
    bottom_right_.set_width(bottom_right_.width() + right_width);
    bottom_right_.set_height(bottom_right_.height() + bottom_width);
  }
}

static inline float CornerRectIntercept(float y, const FloatRect& corner_rect) {
  DCHECK_GT(corner_rect.height(), 0);
  return corner_rect.width() *
         sqrt(1 - (y * y) / (corner_rect.height() * corner_rect.height()));
}

FloatRect FloatRoundedRect::RadiusCenterRect() const {
  FloatRectOutsets maximum_radius_insets(
      -std::max(radii_.TopLeft().height(), radii_.TopRight().height()),
      -std::max(radii_.TopRight().width(), radii_.BottomRight().width()),
      -std::max(radii_.BottomLeft().height(), radii_.BottomRight().height()),
      -std::max(radii_.TopLeft().width(), radii_.BottomLeft().width()));
  FloatRect center_rect(rect_);
  center_rect.Expand(maximum_radius_insets);
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

  const FloatRect& top_left_rect = TopLeftCorner();
  const FloatRect& bottom_left_rect = BottomLeftCorner();

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

  const FloatRect& top_right_rect = TopRightCorner();
  const FloatRect& bottom_right_rect = BottomRightCorner();

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

void FloatRoundedRect::InflateWithRadii(int size) {
  FloatRect old = rect_;

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

bool FloatRoundedRect::IntersectsQuad(const FloatQuad& quad) const {
  if (!quad.IntersectsRect(rect_))
    return false;

  const FloatSize& top_left = radii_.TopLeft();
  if (!top_left.IsEmpty()) {
    FloatRect rect(rect_.x(), rect_.y(), top_left.width(), top_left.height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.x() + top_left.width(),
                        rect_.y() + top_left.height());
      FloatSize size(top_left.width(), top_left.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& top_right = radii_.TopRight();
  if (!top_right.IsEmpty()) {
    FloatRect rect(rect_.right() - top_right.width(), rect_.y(),
                   top_right.width(), top_right.height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.right() - top_right.width(),
                        rect_.y() + top_right.height());
      FloatSize size(top_right.width(), top_right.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& bottom_left = radii_.BottomLeft();
  if (!bottom_left.IsEmpty()) {
    FloatRect rect(rect_.x(), rect_.bottom() - bottom_left.height(),
                   bottom_left.width(), bottom_left.height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.x() + bottom_left.width(),
                        rect_.bottom() - bottom_left.height());
      FloatSize size(bottom_left.width(), bottom_left.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& bottom_right = radii_.BottomRight();
  if (!bottom_right.IsEmpty()) {
    FloatRect rect(rect_.right() - bottom_right.width(),
                   rect_.bottom() - bottom_right.height(), bottom_right.width(),
                   bottom_right.height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.right() - bottom_right.width(),
                        rect_.bottom() - bottom_right.height());
      FloatSize size(bottom_right.width(), bottom_right.height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  return true;
}

float CalcBorderRadiiConstraintScaleFor(const FloatRect& rect,
                                        const FloatRoundedRect::Radii& radii) {
  float factor = 1;
  float radii_sum;

  // top
  radii_sum = radii.TopLeft().width() +
              radii.TopRight().width();  // Casts to avoid integer overflow.
  if (radii_sum > rect.width())
    factor = std::min(rect.width() / radii_sum, factor);

  // bottom
  radii_sum = radii.BottomLeft().width() + radii.BottomRight().width();
  if (radii_sum > rect.width())
    factor = std::min(rect.width() / radii_sum, factor);

  // left
  radii_sum = radii.TopLeft().height() + radii.BottomLeft().height();
  if (radii_sum > rect.height())
    factor = std::min(rect.height() / radii_sum, factor);

  // right
  radii_sum = radii.TopRight().height() + radii.BottomRight().height();
  if (radii_sum > rect.height())
    factor = std::min(rect.height() / radii_sum, factor);

  DCHECK_LE(factor, 1);
  return factor;
}

void FloatRoundedRect::ConstrainRadii() {
  radii_.ScaleAndFloor(CalcBorderRadiiConstraintScaleFor(Rect(), GetRadii()));
}

bool FloatRoundedRect::IsRenderable() const {
  // FIXME: remove the 0.0001 slop once this class is converted to layout units.
  return radii_.TopLeft().width() + radii_.TopRight().width() <=
             rect_.width() + 0.0001 &&
         radii_.BottomLeft().width() + radii_.BottomRight().width() <=
             rect_.width() + 0.0001 &&
         radii_.TopLeft().height() + radii_.BottomLeft().height() <=
             rect_.height() + 0.0001 &&
         radii_.TopRight().height() + radii_.BottomRight().height() <=
             rect_.height() + 0.0001;
}

void FloatRoundedRect::AdjustRadii() {
  float max_radius_width =
      std::max(radii_.TopLeft().width() + radii_.TopRight().width(),
               radii_.BottomLeft().width() + radii_.BottomRight().width());
  float max_radius_height =
      std::max(radii_.TopLeft().height() + radii_.BottomLeft().height(),
               radii_.TopRight().height() + radii_.BottomRight().height());

  if (max_radius_width <= 0 || max_radius_height <= 0) {
    radii_.Scale(0.0f);
    return;
  }
  float width_ratio = static_cast<float>(rect_.width()) / max_radius_width;
  float height_ratio = static_cast<float>(rect_.height()) / max_radius_height;
  radii_.Scale(width_ratio < height_ratio ? width_ratio : height_ratio);
}

std::ostream& operator<<(std::ostream& ostream, const FloatRoundedRect& rect) {
  return ostream << rect.ToString();
}

std::ostream& operator<<(std::ostream& ostream,
                         const FloatRoundedRect::Radii& radii) {
  return ostream << radii.ToString();
}

String FloatRoundedRect::Radii::ToString() const {
  return "tl:" + TopLeft().ToString() + "; tr:" + TopRight().ToString() +
         "; bl:" + BottomLeft().ToString() + "; br:" + BottomRight().ToString();
}

String FloatRoundedRect::ToString() const {
  if (Rect() == FloatRect(LayoutRect::InfiniteIntRect()))
    return "InfiniteIntRect";
  if (GetRadii().IsZero())
    return Rect().ToString();
  return Rect().ToString() + " radii:(" + GetRadii().ToString() + ")";
}

}  // namespace blink
