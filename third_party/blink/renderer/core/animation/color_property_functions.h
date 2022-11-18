// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COLOR_PROPERTY_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COLOR_PROPERTY_FUNCTIONS_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_color.h"

namespace blink {

class ComputedStyle;
class ComputedStyleBuilder;
class CSSProperty;

struct OptionalStyleColor {
 public:
  OptionalStyleColor(std::nullptr_t) : is_null_(true) {}
  OptionalStyleColor(const StyleColor& style_color)
      : is_null_(false), style_color_(style_color) {}
  OptionalStyleColor(const Color& color)
      : is_null_(false), style_color_(color) {}

  bool IsNull() const { return is_null_; }
  const StyleColor& Access() const {
    DCHECK(!is_null_);
    return style_color_;
  }
  bool operator==(const OptionalStyleColor& other) const {
    return is_null_ == other.is_null_ && style_color_ == other.style_color_;
  }

 private:
  bool is_null_;
  StyleColor style_color_;
};

class ColorPropertyFunctions {
 public:
  static OptionalStyleColor GetInitialColor(const CSSProperty&,
                                            const ComputedStyle& initial_style);
  template <typename ComputedStyleOrBuilder>
  static OptionalStyleColor GetUnvisitedColor(const CSSProperty&,
                                              const ComputedStyleOrBuilder&);
  template <typename ComputedStyleOrBuilder>
  static OptionalStyleColor GetVisitedColor(const CSSProperty&,
                                            const ComputedStyleOrBuilder&);
  static void SetUnvisitedColor(const CSSProperty&,
                                ComputedStyleBuilder&,
                                const Color&);
  static void SetVisitedColor(const CSSProperty&,
                              ComputedStyleBuilder&,
                              const Color&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COLOR_PROPERTY_FUNCTIONS_H_
