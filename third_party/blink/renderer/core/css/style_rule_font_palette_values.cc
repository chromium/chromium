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
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"

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
FontPalette::BasePaletteValue StyleRuleFontPaletteValues::GetBasePaletteIndex(
    const Document& document) const {
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
        NOTREACHED();
    }
  }

  MediaValues* media_values =
      MediaValues::CreateDynamicIfFrameExists(document.GetFrame());
  int index =
      To<CSSPrimitiveValue>(*base_palette).ComputeInteger(*media_values);
  return FontPalette::BasePaletteValue({FontPalette::kIndexBasePalette, index});
}

Vector<FontPalette::FontPaletteOverride>
StyleRuleFontPaletteValues::GetOverrideColorsAsVector(
    const Document& document) const {
  const CSSValue* override_colors = GetOverrideColors();
  if (!override_colors || !override_colors->IsValueList()) {
    return {};
  }
  // Colors depending on color scheme are not valid here, so this value
  // doesn't matter.
  // https://drafts.csswg.org/css-fonts/#override-color
  const mojom::blink::ColorScheme used_color_scheme =
      mojom::blink::ColorScheme::kLight;
  MediaValues* media_values =
      MediaValues::CreateDynamicIfFrameExists(document.GetFrame());
  const ResolveColorValueContext color_context{
      .length_resolver = *media_values,
      .text_link_colors = document.GetTextLinkColors(),
      .used_color_scheme = used_color_scheme,
      .color_provider = document.GetColorProviderForPainting(used_color_scheme),
      .is_in_web_app_scope = document.IsInWebAppScope(),
      .for_visited_link = false};

  auto ConvertToColor =
      [&color_context](
          const CSSValuePair& override_pair) -> std::optional<Color> {
    const CSSValue& color_value = override_pair.Second();

    if (color_value.IsIdentifierValue()) {
      const CSSIdentifierValue& color_identifier =
          To<CSSIdentifierValue>(color_value);
      return StyleColor::ColorFromKeyword(
          color_identifier.GetValueID(), color_context.used_color_scheme,
          color_context.color_provider, color_context.is_in_web_app_scope);
    }
    if (const cssvalue::CSSColor* css_color =
            DynamicTo<cssvalue::CSSColor>(color_value)) {
      return css_color->Value();
    }
    StyleColor style_color = ResolveColorValue(color_value, color_context);
    if (style_color.IsAbsoluteColor()) {
      return style_color.GetColor();
    }
    return std::nullopt;
  };

  Vector<FontPalette::FontPaletteOverride> return_overrides;
  const CSSValueList& overrides_list = To<CSSValueList>(*override_colors);
  for (auto& item : overrides_list) {
    const CSSValuePair& override_pair = To<CSSValuePair>(*item);

    const CSSPrimitiveValue& palette_index =
        To<CSSPrimitiveValue>(override_pair.First());
    DCHECK(palette_index.IsInteger());

    std::optional<const Color> override_color = ConvertToColor(override_pair);
    if (!override_color.has_value()) {
      // See comment in ConvertToColor() above.
      continue;
    }
    FontPalette::FontPaletteOverride palette_override{
        ClampTo<uint16_t>(palette_index.ComputeInteger(*media_values)),
        override_color.value()};
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
