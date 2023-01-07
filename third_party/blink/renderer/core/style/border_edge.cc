// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/border_edge.h"

#include <math.h>

namespace blink {

BorderEdge::BorderEdge(float edge_width,
                       const Color& edge_color,
                       EBorderStyle edge_style,
                       bool edge_is_present)
    : color(edge_color),
      is_present(edge_is_present),
      style(static_cast<unsigned>(edge_style)),
      width_(edge_width) {
  if (style == static_cast<unsigned>(EBorderStyle::kDouble) && edge_width < 3)
    style = static_cast<unsigned>(EBorderStyle::kSolid);
}

BorderEdge::BorderEdge()
    : is_present(false), style(static_cast<unsigned>(EBorderStyle::kHidden)) {}

bool BorderEdge::HasVisibleColorAndStyle() const {
  return style > static_cast<unsigned>(EBorderStyle::kHidden) &&
         color.Alpha() > 0;
}

bool BorderEdge::ShouldRender() const {
  return is_present && width_ && HasVisibleColorAndStyle();
}

bool BorderEdge::PresentButInvisible() const {
  return UsedWidth() && !HasVisibleColorAndStyle();
}

bool BorderEdge::ObscuresBackgroundEdge() const {
  if (!is_present || color.HasAlpha() ||
      style == static_cast<unsigned>(EBorderStyle::kHidden))
    return false;

  if (style == static_cast<unsigned>(EBorderStyle::kDotted) ||
      style == static_cast<unsigned>(EBorderStyle::kDashed))
    return false;

  return true;
}

bool BorderEdge::ObscuresBackground() const {
  if (!is_present || color.HasAlpha() ||
      style == static_cast<unsigned>(EBorderStyle::kHidden))
    return false;

  if (style == static_cast<unsigned>(EBorderStyle::kDotted) ||
      style == static_cast<unsigned>(EBorderStyle::kDashed) ||
      style == static_cast<unsigned>(EBorderStyle::kDouble))
    return false;

  return true;
}

float BorderEdge::UsedWidth() const {
  return is_present ? width_ : 0;
}

float BorderEdge::GetDoubleBorderStripeWidth(DoubleBorderStripe stripe) const {
  DCHECK(stripe == kDoubleBorderStripeOuter ||
         stripe == kDoubleBorderStripeInner);

  return roundf(stripe == kDoubleBorderStripeOuter ? UsedWidth() / 3
                                                   : (UsedWidth() * 2) / 3);
}

bool BorderEdge::SharesColorWith(const BorderEdge& other) const {
  return color == other.color;
}

}  // namespace blink
