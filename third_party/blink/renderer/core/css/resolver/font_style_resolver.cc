// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"

#include <optional>

#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {
namespace {

bool ContainsElementDependentCalc(const CSSValue* value) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    return primitive_value->IsCalculated() &&
           To<CSSMathFunctionValue>(*primitive_value).IsElementDependent();
  }
  if (const auto* style_range =
          DynamicTo<cssvalue::CSSFontStyleRangeValue>(value)) {
    if (const CSSValueList* oblique_values = style_range->GetObliqueValues()) {
      for (const CSSValue* v : *oblique_values) {
        if (const auto* pv = DynamicTo<CSSPrimitiveValue>(v)) {
          if (pv->IsCalculated() &&
              To<CSSMathFunctionValue>(*pv).IsElementDependent()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace

std::optional<FontDescription> FontStyleResolver::ComputeFont(
    const CSSPropertyValueSet& property_set,
    FontSelector* font_selector) {
  FontBuilder builder(nullptr);

  FontDescription fontDescription;
  Font* font = MakeGarbageCollected<Font>(fontDescription, font_selector);
  CSSToLengthConversionData::FontSizes font_sizes(10, 10, font, 1);
  CSSToLengthConversionData::LineHeightSize line_height_size;
  CSSToLengthConversionData::ViewportSize viewport_size(0, 0);
  CSSToLengthConversionData::ContainerSizes container_sizes;
  CSSToLengthConversionData::AnchorData anchor_data;
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData conversion_data(
      WritingMode::kHorizontalTb, font_sizes, line_height_size, viewport_size,
      container_sizes, anchor_data, 1, ignored_flags,
      /*element=*/nullptr);

  // CSSPropertyID::kFontSize
  if (property_set.HasProperty(CSSPropertyID::kFontSize)) {
    const CSSValue* value =
        property_set.GetPropertyCSSValue(CSSPropertyID::kFontSize);
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kMath) {
      builder.SetSize(FontDescription::Size(0, 0.0f, false));
    } else if (ContainsElementDependentCalc(value)) {
      return std::nullopt;
    } else {
      builder.SetSize(StyleBuilderConverterBase::ConvertFontSize(
          *value, conversion_data, FontDescription::Size(0, 0.0f, false),
          nullptr));
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
    const CSSValue* value =
        property_set.GetPropertyCSSValue(CSSPropertyID::kFontStretch);
    if (ContainsElementDependentCalc(value)) {
      return std::nullopt;
    } else {
      builder.SetStretch(StyleBuilderConverterBase::ConvertFontStretch(
          conversion_data, *value));
    }
  }

  // CSSPropertyID::kFontStyle
  if (property_set.HasProperty(CSSPropertyID::kFontStyle)) {
    const CSSValue* value =
        property_set.GetPropertyCSSValue(CSSPropertyID::kFontStyle);
    if (ContainsElementDependentCalc(value)) {
      return std::nullopt;
    } else {
      builder.SetStyle(
          StyleBuilderConverterBase::ConvertFontStyle(conversion_data, *value));
    }
  }

  // CSSPropertyID::kFontVariantCaps
  if (property_set.HasProperty(CSSPropertyID::kFontVariantCaps)) {
    builder.SetVariantCaps(StyleBuilderConverterBase::ConvertFontVariantCaps(
        *property_set.GetPropertyCSSValue(CSSPropertyID::kFontVariantCaps)));
  }

  // CSSPropertyID::kFontWeight
  if (property_set.HasProperty(CSSPropertyID::kFontWeight)) {
    const CSSValue* value =
        property_set.GetPropertyCSSValue(CSSPropertyID::kFontWeight);
    if (ContainsElementDependentCalc(value)) {
      return std::nullopt;
    } else {
      builder.SetWeight(StyleBuilderConverterBase::ConvertFontWeight(
          conversion_data, *value, FontBuilder::InitialWeight()));
    }
  }

  builder.UpdateFontDescription(fontDescription);

  return fontDescription;
}

}  // namespace blink
