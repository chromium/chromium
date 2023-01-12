// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COLOR_PROPERTY_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COLOR_PROPERTY_FUNCTIONS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_color.h"

namespace blink {

class ComputedStyle;
class ComputedStyleBuilder;
class CSSProperty;

class ColorPropertyFunctions {
 public:
  static absl::optional<StyleColor> GetInitialColor(
      const CSSProperty&,
      const ComputedStyle& initial_style);
  template <typename ComputedStyleOrBuilder>
  static absl::optional<StyleColor> GetUnvisitedColor(
      const CSSProperty&,
      const ComputedStyleOrBuilder&);
  template <typename ComputedStyleOrBuilder>
  static absl::optional<StyleColor> GetVisitedColor(
      const CSSProperty&,
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
