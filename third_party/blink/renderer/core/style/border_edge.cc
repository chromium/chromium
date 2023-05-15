// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/border_edge.h"

#include <math.h>

namespace blink {

BorderEdge::BorderEdge(int edge_width,
                       const Color& edge_color,
                       EBorderStyle edge_style,
                       bool edge_is_present)
    : color_(edge_color),
      is_present_(edge_is_present),
      style_(EffectiveStyle(edge_style, edge_width)),
      width_(edge_width) {}

BorderEdge::BorderEdge() : is_present_(false), style_(EBorderStyle::kHidden) {}

// static
EBorderStyle BorderEdge::EffectiveStyle(EBorderStyle style, int width) {
  if ((style == EBorderStyle::kDouble && width < 3) ||
      ((style == EBorderStyle::kRidge || style == EBorderStyle::kGroove) &&
       width <= 1)) {
    return EBorderStyle::kSolid;
  }
  return style;
}

bool BorderEdge::HasVisibleColorAndStyle() const {
  return style_ > EBorderStyle::kHidden && !color_.IsFullyTransparent();
}

bool BorderEdge::ShouldRender() const {
  return is_present_ && width_ && HasVisibleColorAndStyle();
}

bool BorderEdge::PresentButInvisible() const {
  return UsedWidth() && !HasVisibleColorAndStyle();
}

bool BorderEdge::ObscuresBackgroundEdge() const {
  if (!is_present_ || !color_.IsOpaque() || style_ == EBorderStyle::kHidden) {
    return false;
  }

  if (style_ == EBorderStyle::kDotted || style_ == EBorderStyle::kDashed) {
    return false;
  }

  return true;
}

bool BorderEdge::ObscuresBackground() const {
  if (!is_present_ || !color_.IsOpaque() || style_ == EBorderStyle::kHidden) {
    return false;
  }

  if (style_ == EBorderStyle::kDotted || style_ == EBorderStyle::kDashed ||
      style_ == EBorderStyle::kDouble) {
    return false;
  }

  return true;
}

int BorderEdge::UsedWidth() const {
  return is_present_ ? width_ : 0;
}

int BorderEdge::GetDoubleBorderStripeWidth(DoubleBorderStripe stripe) const {
  DCHECK(stripe == kDoubleBorderStripeOuter ||
         stripe == kDoubleBorderStripeInner);

  return roundf(stripe == kDoubleBorderStripeOuter
                    ? UsedWidth() / 3.0f
                    : (UsedWidth() * 2.0f) / 3.0f);
}

bool BorderEdge::SharesColorWith(const BorderEdge& other) const {
  return color_ == other.color_;
}

void BorderEdge::ClampWidth(int max_width) {
  if (width_ > max_width) {
    width_ = max_width;
    style_ = EffectiveStyle(style_, width_);
  }
}

}  // namespace blink
