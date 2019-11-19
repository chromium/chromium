// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

FontDescription FontStyleResolver::ComputeFont(
    const CSSPropertyValueSet& property_set,
    FontSelector* font_selector) {
  FontBuilder builder(nullptr);

  FontDescription fontDescription;
  Font font(fontDescription);
  font.Update(font_selector);
  CSSToLengthConversionData::FontSizes fontSizes(10, 10, &font, 1);
  CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
  CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize, 1);

  // CSSPropertyID::kFontSize
  if (property_set.HasProperty(CSSPropertyID::kFontSize)) {
    builder.SetSize(StyleBuilderConverterBase::ConvertFontSize(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontSize),
        conversionData, FontDescription::Size(0, 0.0f, false)));
  }

  // CSSPropertyID::kFontFamily
  if (property_set.HasProperty(CSSPropertyID::kFontFamily)) {
    builder.SetFamilyDescription(StyleBuilderConverterBase::ConvertFontFamily(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontFamily), &builder,
        nullptr));
  }

  // CSSPropertyID::kFontStretch
  if (property_set.HasProperty(CSSPropertyID::kFontStretch)) {
    builder.SetStretch(StyleBuilderConverterBase::ConvertFontStretch(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontStretch)));
  }

  // CSSPropertyID::kFontStyle
  if (property_set.HasProperty(CSSPropertyID::kFontStyle)) {
    builder.SetStyle(StyleBuilderConverterBase::ConvertFontStyle(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontStyle)));
  }

  // CSSPropertyID::kFontVariantCaps
  if (property_set.HasProperty(CSSPropertyID::kFontVariantCaps)) {
    builder.SetVariantCaps(StyleBuilderConverterBase::ConvertFontVariantCaps(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontVariantCaps)));
  }

  // CSSPropertyID::kFontWeight
  if (property_set.HasProperty(CSSPropertyID::kFontWeight)) {
    builder.SetWeight(StyleBuilderConverterBase::ConvertFontWeight(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontWeight),
        FontBuilder::InitialWeight()));
  }

  builder.UpdateFontDescription(fontDescription);

  return fontDescription;
}

}  // namespace blink
