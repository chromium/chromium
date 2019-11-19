// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_AUTO_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_AUTO_COLOR_H_

#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StyleAutoColor {
  DISALLOW_NEW();

 public:
  StyleAutoColor(Color color)
      : type_(ValueType::kSpecifiedColor), color_(color) {}
  static StyleAutoColor AutoColor() { return StyleAutoColor(ValueType::kAuto); }
  static StyleAutoColor CurrentColor() {
    return StyleAutoColor(ValueType::kCurrentColor);
  }

  bool IsAutoColor() const { return type_ == ValueType::kAuto; }
  bool IsCurrentColor() const { return type_ == ValueType::kCurrentColor; }
  Color GetColor() const {
    DCHECK(type_ == ValueType::kSpecifiedColor);
    return color_;
  }

  Color Resolve(Color current_color) const {
    return type_ == ValueType::kSpecifiedColor ? color_ : current_color;
  }

  StyleColor ToStyleColor() const {
    DCHECK(type_ != ValueType::kAuto);
    if (type_ == ValueType::kSpecifiedColor)
      return StyleColor(color_);
    DCHECK(type_ == ValueType::kCurrentColor);
    return StyleColor::CurrentColor();
  }

 private:
  enum class ValueType { kAuto, kCurrentColor, kSpecifiedColor };
  StyleAutoColor(ValueType type) : type_(type) {}

  ValueType type_;
  Color color_;
};

inline bool operator==(const StyleAutoColor& a, const StyleAutoColor& b) {
  if (a.IsAutoColor() || b.IsAutoColor())
    return a.IsAutoColor() && b.IsAutoColor();
  if (a.IsCurrentColor() || b.IsCurrentColor())
    return a.IsCurrentColor() && b.IsCurrentColor();
  return a.GetColor() == b.GetColor();
}

inline bool operator!=(const StyleAutoColor& a, const StyleAutoColor& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_AUTO_COLOR_H_
