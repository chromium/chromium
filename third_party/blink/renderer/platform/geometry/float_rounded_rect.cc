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
  top_left_.SetWidth(std::max(minimum_radius, top_left_.Width()));
  top_left_.SetHeight(std::max(minimum_radius, top_left_.Height()));
  top_right_.SetWidth(std::max(minimum_radius, top_right_.Width()));
  top_right_.SetHeight(std::max(minimum_radius, top_right_.Height()));
  bottom_left_.SetWidth(std::max(minimum_radius, bottom_left_.Width()));
  bottom_left_.SetHeight(std::max(minimum_radius, bottom_left_.Height()));
  bottom_right_.SetWidth(std::max(minimum_radius, bottom_right_.Width()));
  bottom_right_.SetHeight(std::max(minimum_radius, bottom_right_.Height()));
}

absl::optional<float> FloatRoundedRect::Radii::UniformRadius() const {
  if (top_left_.Width() == top_left_.Height() && top_left_ == top_right_ &&
      top_left_ == bottom_left_ && top_left_ == bottom_right_) {
    return top_left_.Width();
  }
  return absl::nullopt;
}

void FloatRoundedRect::Radii::Scale(float factor) {
  if (factor == 1)
    return;

  // If either radius on a corner becomes zero, reset both radii on that corner.
  top_left_.Scale(factor);
  if (!top_left_.Width() || !top_left_.Height())
    top_left_ = FloatSize();
  top_right_.Scale(factor);
  if (!top_right_.Width() || !top_right_.Height())
    top_right_ = FloatSize();
  bottom_left_.Scale(factor);
  if (!bottom_left_.Width() || !bottom_left_.Height())
    bottom_left_ = FloatSize();
  bottom_right_.Scale(factor);
  if (!bottom_right_.Width() || !bottom_right_.Height())
    bottom_right_ = FloatSize();
}

void FloatRoundedRect::Radii::ScaleAndFloor(float factor) {
  if (factor == 1)
    return;

  // If either radius on a corner becomes zero, reset both radii on that corner.
  top_left_.ScaleAndFloor(factor);
  if (!top_left_.Width() || !top_left_.Height())
    top_left_ = FloatSize();
  top_right_.ScaleAndFloor(factor);
  if (!top_right_.Width() || !top_right_.Height())
    top_right_ = FloatSize();
  bottom_left_.ScaleAndFloor(factor);
  if (!bottom_left_.Width() || !bottom_left_.Height())
    bottom_left_ = FloatSize();
  bottom_right_.ScaleAndFloor(factor);
  if (!bottom_right_.Width() || !bottom_right_.Height())
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

  top_left_.SetWidth(std::max<float>(0, top_left_.Width() - left_width));
  top_left_.SetHeight(std::max<float>(0, top_left_.Height() - top_width));

  top_right_.SetWidth(std::max<float>(0, top_right_.Width() - right_width));
  top_right_.SetHeight(std::max<float>(0, top_right_.Height() - top_width));

  bottom_left_.SetWidth(std::max<float>(0, bottom_left_.Width() - left_width));
  bottom_left_.SetHeight(
      std::max<float>(0, bottom_left_.Height() - bottom_width));

  bottom_right_.SetWidth(
      std::max<float>(0, bottom_right_.Width() - right_width));
  bottom_right_.SetHeight(
      std::max<float>(0, bottom_right_.Height() - bottom_width));
}

void FloatRoundedRect::Radii::Expand(float top_width,
                                     float bottom_width,
                                     float left_width,
                                     float right_width) {
  DCHECK_GE(top_width, 0);
  DCHECK_GE(bottom_width, 0);
  DCHECK_GE(left_width, 0);
  DCHECK_GE(right_width, 0);
  if (top_left_.Width() > 0 && top_left_.Height() > 0) {
    top_left_.SetWidth(top_left_.Width() + left_width);
    top_left_.SetHeight(top_left_.Height() + top_width);
  }
  if (top_right_.Width() > 0 && top_right_.Height() > 0) {
    top_right_.SetWidth(top_right_.Width() + right_width);
    top_right_.SetHeight(top_right_.Height() + top_width);
  }
  if (bottom_left_.Width() > 0 && bottom_left_.Height() > 0) {
    bottom_left_.SetWidth(bottom_left_.Width() + left_width);
    bottom_left_.SetHeight(bottom_left_.Height() + bottom_width);
  }
  if (bottom_right_.Width() > 0 && bottom_right_.Height() > 0) {
    bottom_right_.SetWidth(bottom_right_.Width() + right_width);
    bottom_right_.SetHeight(bottom_right_.Height() + bottom_width);
  }
}

static inline float CornerRectIntercept(float y, const FloatRect& corner_rect) {
  DCHECK_GT(corner_rect.Height(), 0);
  return corner_rect.Width() *
         sqrt(1 - (y * y) / (corner_rect.Height() * corner_rect.Height()));
}

FloatRect FloatRoundedRect::RadiusCenterRect() const {
  FloatRectOutsets maximum_radius_insets(
      -std::max(radii_.TopLeft().Height(), radii_.TopRight().Height()),
      -std::max(radii_.TopRight().Width(), radii_.BottomRight().Width()),
      -std::max(radii_.BottomLeft().Height(), radii_.BottomRight().Height()),
      -std::max(radii_.TopLeft().Width(), radii_.BottomLeft().Width()));
  FloatRect center_rect(rect_);
  center_rect.Expand(maximum_radius_insets);
  return center_rect;
}

bool FloatRoundedRect::XInterceptsAtY(float y,
                                      float& min_x_intercept,
                                      float& max_x_intercept) const {
  if (y < Rect().Y() || y > Rect().MaxY())
    return false;

  if (!IsRounded()) {
    min_x_intercept = Rect().X();
    max_x_intercept = Rect().MaxX();
    return true;
  }

  const FloatRect& top_left_rect = TopLeftCorner();
  const FloatRect& bottom_left_rect = BottomLeftCorner();

  if (!top_left_rect.IsEmpty() && y >= top_left_rect.Y() &&
      y < top_left_rect.MaxY())
    min_x_intercept =
        top_left_rect.MaxX() -
        CornerRectIntercept(top_left_rect.MaxY() - y, top_left_rect);
  else if (!bottom_left_rect.IsEmpty() && y >= bottom_left_rect.Y() &&
           y <= bottom_left_rect.MaxY())
    min_x_intercept =
        bottom_left_rect.MaxX() -
        CornerRectIntercept(y - bottom_left_rect.Y(), bottom_left_rect);
  else
    min_x_intercept = rect_.X();

  const FloatRect& top_right_rect = TopRightCorner();
  const FloatRect& bottom_right_rect = BottomRightCorner();

  if (!top_right_rect.IsEmpty() && y >= top_right_rect.Y() &&
      y <= top_right_rect.MaxY())
    max_x_intercept =
        top_right_rect.X() +
        CornerRectIntercept(top_right_rect.MaxY() - y, top_right_rect);
  else if (!bottom_right_rect.IsEmpty() && y >= bottom_right_rect.Y() &&
           y <= bottom_right_rect.MaxY())
    max_x_intercept =
        bottom_right_rect.X() +
        CornerRectIntercept(y - bottom_right_rect.Y(), bottom_right_rect);
  else
    max_x_intercept = rect_.MaxX();

  return true;
}

void FloatRoundedRect::InflateWithRadii(int size) {
  FloatRect old = rect_;

  rect_.Inflate(size);
  // Considering the inflation factor of shorter size to scale the radii seems
  // appropriate here
  float factor;
  if (rect_.Width() < rect_.Height())
    factor = old.Width() ? (float)rect_.Width() / old.Width() : int(0);
  else
    factor = old.Height() ? (float)rect_.Height() / old.Height() : int(0);

  radii_.Scale(factor);
}

bool FloatRoundedRect::IntersectsQuad(const FloatQuad& quad) const {
  if (!quad.IntersectsRect(rect_))
    return false;

  const FloatSize& top_left = radii_.TopLeft();
  if (!top_left.IsEmpty()) {
    FloatRect rect(rect_.X(), rect_.Y(), top_left.Width(), top_left.Height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.X() + top_left.Width(),
                        rect_.Y() + top_left.Height());
      FloatSize size(top_left.Width(), top_left.Height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& top_right = radii_.TopRight();
  if (!top_right.IsEmpty()) {
    FloatRect rect(rect_.MaxX() - top_right.Width(), rect_.Y(),
                   top_right.Width(), top_right.Height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.MaxX() - top_right.Width(),
                        rect_.Y() + top_right.Height());
      FloatSize size(top_right.Width(), top_right.Height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& bottom_left = radii_.BottomLeft();
  if (!bottom_left.IsEmpty()) {
    FloatRect rect(rect_.X(), rect_.MaxY() - bottom_left.Height(),
                   bottom_left.Width(), bottom_left.Height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.X() + bottom_left.Width(),
                        rect_.MaxY() - bottom_left.Height());
      FloatSize size(bottom_left.Width(), bottom_left.Height());
      if (!quad.IntersectsEllipse(center, size))
        return false;
    }
  }

  const FloatSize& bottom_right = radii_.BottomRight();
  if (!bottom_right.IsEmpty()) {
    FloatRect rect(rect_.MaxX() - bottom_right.Width(),
                   rect_.MaxY() - bottom_right.Height(), bottom_right.Width(),
                   bottom_right.Height());
    if (quad.IntersectsRect(rect)) {
      FloatPoint center(rect_.MaxX() - bottom_right.Width(),
                        rect_.MaxY() - bottom_right.Height());
      FloatSize size(bottom_right.Width(), bottom_right.Height());
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
  radii_sum = radii.TopLeft().Width() +
              radii.TopRight().Width();  // Casts to avoid integer overflow.
  if (radii_sum > rect.Width())
    factor = std::min(rect.Width() / radii_sum, factor);

  // bottom
  radii_sum = radii.BottomLeft().Width() + radii.BottomRight().Width();
  if (radii_sum > rect.Width())
    factor = std::min(rect.Width() / radii_sum, factor);

  // left
  radii_sum = radii.TopLeft().Height() + radii.BottomLeft().Height();
  if (radii_sum > rect.Height())
    factor = std::min(rect.Height() / radii_sum, factor);

  // right
  radii_sum = radii.TopRight().Height() + radii.BottomRight().Height();
  if (radii_sum > rect.Height())
    factor = std::min(rect.Height() / radii_sum, factor);

  DCHECK_LE(factor, 1);
  return factor;
}

void FloatRoundedRect::ConstrainRadii() {
  radii_.ScaleAndFloor(CalcBorderRadiiConstraintScaleFor(Rect(), GetRadii()));
}

bool FloatRoundedRect::IsRenderable() const {
  // FIXME: remove the 0.0001 slop once this class is converted to layout units.
  return radii_.TopLeft().Width() + radii_.TopRight().Width() <=
             rect_.Width() + 0.0001 &&
         radii_.BottomLeft().Width() + radii_.BottomRight().Width() <=
             rect_.Width() + 0.0001 &&
         radii_.TopLeft().Height() + radii_.BottomLeft().Height() <=
             rect_.Height() + 0.0001 &&
         radii_.TopRight().Height() + radii_.BottomRight().Height() <=
             rect_.Height() + 0.0001;
}

void FloatRoundedRect::AdjustRadii() {
  float max_radius_width =
      std::max(radii_.TopLeft().Width() + radii_.TopRight().Width(),
               radii_.BottomLeft().Width() + radii_.BottomRight().Width());
  float max_radius_height =
      std::max(radii_.TopLeft().Height() + radii_.BottomLeft().Height(),
               radii_.TopRight().Height() + radii_.BottomRight().Height());

  if (max_radius_width <= 0 || max_radius_height <= 0) {
    radii_.Scale(0.0f);
    return;
  }
  float width_ratio = static_cast<float>(rect_.Width()) / max_radius_width;
  float height_ratio = static_cast<float>(rect_.Height()) / max_radius_height;
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
