/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_VALUE_H_

#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// In order to conserve memory, the border width uses fixed point,
// which can be bitpacked.  This fixed point implementation is
// essentially the same as in LayoutUnit.  Six bits are used for the
// fraction, which leaves 20 bits for the integer part, making 1048575
// the largest number.

static const int kBorderWidthFractionalBits = 6;
static const int kBorderWidthDenominator = 1 << kBorderWidthFractionalBits;
static const int kMaxForBorderWidth = ((1 << 26) - 1) / kBorderWidthDenominator;

class BorderValue {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  BorderValue()
      : color_(0),
        color_is_current_color_(true),
        style_(static_cast<unsigned>(EBorderStyle::kNone)) {
    SetWidth(3);
  }

  BorderValue(EBorderStyle style, const StyleColor& color, float width) {
    SetColor(color);
    SetStyle(style);
    SetWidth(width);
  }

  bool IsTransparent() const {
    return !color_is_current_color_ && !color_.Alpha();
  }

  bool operator==(const BorderValue& o) const {
    return width_ == o.width_ && style_ == o.style_ && color_ == o.color_ &&
           color_is_current_color_ == o.color_is_current_color_;
  }

  // The default width is 3px, but if the style is none we compute a value of 0
  // (in ComputedStyle itself)
  bool VisuallyEqual(const BorderValue& o) const {
    if (style_ == static_cast<unsigned>(EBorderStyle::kNone) &&
        o.style_ == static_cast<unsigned>(EBorderStyle::kNone))
      return true;
    if (style_ == static_cast<unsigned>(EBorderStyle::kHidden) &&
        o.style_ == static_cast<unsigned>(EBorderStyle::kHidden))
      return true;
    return *this == o;
  }

  bool operator!=(const BorderValue& o) const { return !(*this == o); }

  void SetColor(const StyleColor& color) {
    color_ = color.Resolve(Color());
    color_is_current_color_ = color.IsCurrentColor();
  }

  StyleColor GetColor() const {
    return color_is_current_color_ ? StyleColor::CurrentColor()
                                   : StyleColor(color_);
  }

  float Width() const {
    return static_cast<float>(width_) / kBorderWidthDenominator;
  }
  void SetWidth(float width) { width_ = WidthToFixedPoint(width); }

  // Since precision is lost with fixed point, comparisons also have
  // to be done in fixed point.
  bool WidthEquals(float width) const {
    return WidthToFixedPoint(width) == width_;
  }

  EBorderStyle Style() const { return static_cast<EBorderStyle>(style_); }
  void SetStyle(EBorderStyle style) { style_ = static_cast<unsigned>(style); }

  bool ColorIsCurrentColor() const { return color_is_current_color_; }
  void SetColorIsCurrentColor(bool color_is_current_color) {
    color_is_current_color_ = static_cast<unsigned>(color_is_current_color);
  }

 protected:
  static unsigned WidthToFixedPoint(float width) {
    DCHECK_GE(width, 0);
    // Avoid min()/max() from std here in the header, because that would require
    // inclusion of <algorithm>, which is slow to compile.
    if (width > float(kMaxForBorderWidth))
      width = float(kMaxForBorderWidth);
    return static_cast<unsigned>(width * kBorderWidthDenominator);
  }

  Color color_;
  unsigned color_is_current_color_ : 1;

  unsigned width_ : 26;  // Fixed point width
  unsigned style_ : 4;   // EBorderStyle
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_VALUE_H_
