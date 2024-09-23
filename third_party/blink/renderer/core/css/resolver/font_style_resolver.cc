// Copyright 2017 The Chromium Authors
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
  Font font(fontDescription, font_selector);
  CSSToLengthConversionData::FontSizes font_sizes(10, 10, &font, 1);
  CSSToLengthConversionData::LineHeightSize line_height_size;
  CSSToLengthConversionData::ViewportSize viewport_size(0, 0);
  CSSToLengthConversionData::ContainerSizes container_sizes;
  CSSToLengthConversionData::AnchorData anchor_data;
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData conversion_data(
      WritingMode::kHorizontalTb, font_sizes, line_height_size, viewport_size,
      container_sizes, anchor_data, 1, ignored_flags);

  // CSSPropertyID::kFontSize
  if (property_set.HasProperty(CSSPropertyID::kFontSize)) {
    const CSSValue* value =
        property_set.GetPropertyCSSValue(CSSPropertyID::kFontSize);
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kMath) {
      builder.SetSize(FontDescription::Size(0, 0.0f, false));
    } else {
      builder.SetSize(StyleBuilderConverterBase::ConvertFontSize(
          *property_set.GetPropertyCSSValue(CSSPropertyID::kFontSize),
          conversion_data, FontDescription::Size(0, 0.0f, false), nullptr));
    }
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
        conversion_data,
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontStretch)));
  }

  // CSSPropertyID::kFontStyle
  if (property_set.HasProperty(CSSPropertyID::kFontStyle)) {
    builder.SetStyle(StyleBuilderConverterBase::ConvertFontStyle(
        conversion_data,
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
