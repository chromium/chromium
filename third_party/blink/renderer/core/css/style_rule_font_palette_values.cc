// Copyright 2022 The Chromium Authors
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
    : StyleRuleBase(kFontPaletteValues), name_(name), properties_(properties) {
  DCHECK(properties);
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(
    const StyleRuleFontPaletteValues&) = default;

StyleRuleFontPaletteValues::~StyleRuleFontPaletteValues() = default;

const CSSValue* StyleRuleFontPaletteValues::GetFontFamily() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kFontFamily);
}
const CSSValue* StyleRuleFontPaletteValues::GetBasePalette() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kBasePalette);
}
const CSSValue* StyleRuleFontPaletteValues::GetOverrideColors() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kOverrideColors);
}
FontPalette::BasePaletteValue StyleRuleFontPaletteValues::GetBasePaletteIndex()
    const {
  constexpr FontPalette::BasePaletteValue kNoBasePaletteValue = {
      FontPalette::kNoBasePalette, 0};
  const CSSValue* base_palette = GetBasePalette();
  if (!base_palette) {
    return kNoBasePaletteValue;
  }

  if (auto* base_palette_identifier =
          DynamicTo<CSSIdentifierValue>(*base_palette)) {
    switch (base_palette_identifier->GetValueID()) {
      case CSSValueID::kLight:
        return FontPalette::BasePaletteValue(
            {FontPalette::kLightBasePalette, 0});
      case CSSValueID::kDark:
        return FontPalette::BasePaletteValue(
            {FontPalette::kDarkBasePalette, 0});
      default:
        NOTREACHED_IN_MIGRATION();
        return kNoBasePaletteValue;
    }
  }

  const CSSPrimitiveValue& palette_primitive =
      To<CSSPrimitiveValue>(*base_palette);
  return FontPalette::BasePaletteValue(
      {FontPalette::kIndexBasePalette, palette_primitive.GetIntValue()});
}

Vector<FontPalette::FontPaletteOverride>
StyleRuleFontPaletteValues::GetOverrideColorsAsVector() const {
  const CSSValue* override_colors = GetOverrideColors();
  if (!override_colors || !override_colors->IsValueList()) {
    return {};
  }

  // Note: This function should not allocate Oilpan object, e.g. `CSSValue`,
  // because this function is called in font threads to determine primary
  // font data via `CSSFontSelector::GetFontData()`.
  // The test[1] reaches here.
  // [1] https://wpt.live/css/css-fonts/font-palette-35.html
  // TODO(yosin): Should we use ` ThreadState::NoAllocationScope` for main
  // thread? Font threads hit `DCHECK` because they don't have `ThreadState'.

  auto ConvertToColor = [](const CSSValuePair& override_pair) -> Color {
    if (override_pair.Second().IsIdentifierValue()) {
      const CSSIdentifierValue& color_identifier =
          To<CSSIdentifierValue>(override_pair.Second());
      // The value won't be a system color according to parsing, so we can pass
      // a fixed color scheme, color provider and `false` to indicate that we
      // are not within a WebApp context.
      return StyleColor::ColorFromKeyword(
          color_identifier.GetValueID(), mojom::blink::ColorScheme::kLight,
          /*color_provider=*/nullptr, /*is_in_web_app_scope=*/false);
    }
    const cssvalue::CSSColor& css_color =
        To<cssvalue::CSSColor>(override_pair.Second());
    return css_color.Value();
  };

  Vector<FontPalette::FontPaletteOverride> return_overrides;
  const CSSValueList& overrides_list = To<CSSValueList>(*override_colors);
  for (auto& item : overrides_list) {
    const CSSValuePair& override_pair = To<CSSValuePair>(*item);

    const CSSPrimitiveValue& palette_index =
        To<CSSPrimitiveValue>(override_pair.First());
    DCHECK(palette_index.IsInteger());

    const Color override_color = ConvertToColor(override_pair);

    FontPalette::FontPaletteOverride palette_override{
        palette_index.GetValue<uint16_t>(), override_color};
    return_overrides.push_back(palette_override);
  }

  return return_overrides;
}

MutableCSSPropertyValueSet& StyleRuleFontPaletteValues::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleFontPaletteValues::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
