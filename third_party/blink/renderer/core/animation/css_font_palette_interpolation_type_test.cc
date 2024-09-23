// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_palette_interpolation_type.h"
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/interpolable_font_palette.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class CSSFontPaletteInterpolationTypeTest : public PageTestBase {
 protected:
  std::unique_ptr<CSSFontPaletteInterpolationType>
  CreateFontPaletteInterpolationType() {
    const CSSProperty& css_property =
        CSSProperty::Get(CSSPropertyID::kFontPalette);
    PropertyHandle property = PropertyHandle(css_property);
    return std::make_unique<CSSFontPaletteInterpolationType>(property);
  }
};

TEST_F(CSSFontPaletteInterpolationTypeTest,
       MaybeConvertStandardPropertyUnderlyingValue) {
  SetBodyInnerHTML(R"HTML(
  <style>
    div {
      font-size: 3rem;
      font-family: "family";
      font-palette: --palette1;
      transition: font-palette 2s;
    }
  </style>
  <div id="text">Filler text</div>
  )HTML");
  Document& document = GetDocument();
  Element* element = document.getElementById(AtomicString("text"));
  StyleResolverState state(document, *element, nullptr,
                           StyleRequest(element->GetComputedStyle()));

  std::unique_ptr<CSSFontPaletteInterpolationType>
      font_palette_interpolation_type = CreateFontPaletteInterpolationType();

  InterpolationValue result = font_palette_interpolation_type
                                  ->MaybeConvertStandardPropertyUnderlyingValue(
                                      *element->GetComputedStyle());

  const InterpolableFontPalette* interpolable_font_palette =
      To<InterpolableFontPalette>(result.interpolable_value.Get());
  scoped_refptr<const FontPalette> font_palette =
      interpolable_font_palette->GetFontPalette();

  EXPECT_EQ(font_palette->ToString(), "--palette1");
}

TEST_F(CSSFontPaletteInterpolationTypeTest, MaybeConvertValue) {
  std::unique_ptr<CSSFontPaletteInterpolationType>
      font_palette_interpolation_type = CreateFontPaletteInterpolationType();
  CSSFontPaletteInterpolationType::ConversionCheckers conversion_checkers;
  CSSValue* value =
      MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("--palette"));

  InterpolationValue result =
      font_palette_interpolation_type->MaybeConvertValue(*value, nullptr,
                                                         conversion_checkers);

  const InterpolableFontPalette* interpolable_font_palette =
      To<InterpolableFontPalette>(result.interpolable_value.Get());
  scoped_refptr<const FontPalette> font_palette =
      interpolable_font_palette->GetFontPalette();

  EXPECT_EQ(font_palette->ToString(), "--palette");
}

}  // namespace blink
