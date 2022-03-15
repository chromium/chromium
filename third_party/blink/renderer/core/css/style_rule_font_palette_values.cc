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
#include "third_party/blink/renderer/core/css/style_color.h"

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

AtomicString StyleRuleFontPaletteValues::GetFontFamilyAsString() const {
  if (!font_family_ || !font_family_->IsFontFamilyValue())
    return g_empty_atom;

  return To<CSSFontFamilyValue>(*font_family_).Value();
}

FontPalette::BasePaletteValue StyleRuleFontPaletteValues::GetBasePaletteIndex()
    const {
  constexpr FontPalette::BasePaletteValue kNoBasePaletteValue = {
      FontPalette::kNoBasePalette, 0};
  if (!base_palette_) {
    return kNoBasePaletteValue;
  }

  if (auto* base_palette_identifier =
          DynamicTo<CSSIdentifierValue>(*base_palette_)) {
    switch (base_palette_identifier->GetValueID()) {
      case CSSValueID::kLight:
        return FontPalette::BasePaletteValue(
            {FontPalette::kLightBasePalette, 0});
      case CSSValueID::kDark:
        return FontPalette::BasePaletteValue(
            {FontPalette::kDarkBasePalette, 0});
      default:
        NOTREACHED();
        return kNoBasePaletteValue;
    }
  }

  const CSSPrimitiveValue& palette_primitive =
      To<CSSPrimitiveValue>(*base_palette_);
  return FontPalette::BasePaletteValue(
      {FontPalette::kIndexBasePalette, palette_primitive.GetIntValue()});
}

Vector<FontPalette::FontPaletteOverride>
StyleRuleFontPaletteValues::GetOverrideColorsAsVector() const {
  if (!override_colors_ || !override_colors_->IsValueList())
    return {};

  const CSSValueList& overrides_list = To<CSSValueList>(*override_colors_);

  Vector<FontPalette::FontPaletteOverride> return_overrides;
  for (auto& item : overrides_list) {
    const CSSValuePair& override_pair = To<CSSValuePair>(*item);

    const CSSPrimitiveValue& palette_index =
        To<CSSPrimitiveValue>(override_pair.First());
    DCHECK(palette_index.IsInteger());

    const cssvalue::CSSColor* override_color;
    if (override_pair.Second().IsIdentifierValue()) {
      const CSSIdentifierValue& color_identifier =
          To<CSSIdentifierValue>(override_pair.Second());
      // The value won't be a system color according to parsing, so we can pass
      // a fixed color scheme here.
      override_color = cssvalue::CSSColor::Create(
          StyleColor::ColorFromKeyword(color_identifier.GetValueID(),
                                       mojom::blink::ColorScheme::kLight)
              .Rgb());
    } else {
      override_color = DynamicTo<cssvalue::CSSColor>(override_pair.Second());
      DCHECK(override_color);
    }

    FontPalette::FontPaletteOverride palette_override{
        palette_index.GetIntValue(),
        static_cast<SkColor>(override_color->Value())};
    return_overrides.push_back(palette_override);
  }

  return return_overrides;
}

void StyleRuleFontPaletteValues::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(font_family_);
  visitor->Trace(base_palette_);
  visitor->Trace(override_colors_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
