// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

Color StyleColor::Resolve(Color current_color, ColorScheme color_scheme) const {
  if (IsCurrentColor())
    return current_color;
  if (EffectiveColorKeyword() != CSSValueID::kInvalid)
    return ColorFromKeyword(color_keyword_, color_scheme);
  return color_;
}

Color StyleColor::ResolveWithAlpha(Color current_color,
                                   ColorScheme color_scheme,
                                   int alpha) const {
  Color color = Resolve(current_color, color_scheme);
  return Color(color.Red(), color.Green(), color.Blue(), alpha);
}

Color StyleColor::ColorFromKeyword(CSSValueID keyword,
                                   ColorScheme color_scheme) {
  if (const char* value_name = getValueName(keyword)) {
    if (const NamedColor* named_color =
            FindColor(value_name, static_cast<wtf_size_t>(strlen(value_name))))
      return Color(named_color->argb_value);
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

bool StyleColor::IsColorKeyword(CSSValueID id) {
  // Named colors and color keywords:
  //
  // <named-color>
  //   'aqua', 'black', 'blue', ..., 'yellow' (CSS3: "basic color keywords")
  //   'aliceblue', ..., 'yellowgreen'        (CSS3: "extended color keywords")
  //   'transparent'
  //
  // 'currentcolor'
  //
  // <deprecated-system-color>
  //   'ActiveBorder', ..., 'WindowText'
  //
  // WebKit proprietary/internal:
  //   '-webkit-link'
  //   '-webkit-activelink'
  //   '-internal-active-list-box-selection'
  //   '-internal-active-list-box-selection-text'
  //   '-internal-inactive-list-box-selection'
  //   '-internal-inactive-list-box-selection-text'
  //   '-webkit-focus-ring-color'
  //   '-internal-quirk-inherit'
  //
  return (id >= CSSValueID::kAqua && id <= CSSValueID::kInternalQuirkInherit) ||
         (id >= CSSValueID::kAliceblue && id <= CSSValueID::kYellowgreen) ||
         id == CSSValueID::kMenu;
}

bool StyleColor::IsSystemColor(CSSValueID id) {
  return (id >= CSSValueID::kActiveborder && id <= CSSValueID::kWindowtext) ||
         id == CSSValueID::kMenu;
}

CSSValueID StyleColor::EffectiveColorKeyword() const {
  if (!RuntimeEnabledFeatures::CSSSystemColorComputeToSelfEnabled()) {
    return IsSystemColor(color_keyword_) ? CSSValueID::kInvalid
                                         : color_keyword_;
  }
  return color_keyword_;
}

}  // namespace blink
