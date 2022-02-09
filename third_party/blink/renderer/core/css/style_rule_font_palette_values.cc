// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_palette_values_rule.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(
    const AtomicString& name,
    CSSPropertyValueSet* properties)
    : StyleRuleBase(kFontPaletteValues),
      name_(name),
      font_family_(properties->GetPropertyCSSValue(CSSPropertyID::kFontFamily)),
      base_palette_(
          properties->GetPropertyCSSValue(CSSPropertyID::kBasePalette)),
      override_colors_(
          properties->GetPropertyCSSValue(CSSPropertyID::kOverrideColors)) {
  DCHECK(properties);
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(
    const StyleRuleFontPaletteValues&) = default;

StyleRuleFontPaletteValues::~StyleRuleFontPaletteValues() = default;

void StyleRuleFontPaletteValues::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(font_family_);
  visitor->Trace(base_palette_);
  visitor->Trace(override_colors_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
