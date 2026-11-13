// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"

#include <initializer_list>
#include <optional>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_optical_sizing.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFourByteTag.h"

namespace blink {
namespace {

constexpr SkFourByteTag kAaaaTag = SkSetFourByteTag('A', 'A', 'A', 'A');
constexpr SkFourByteTag kGradTag = SkSetFourByteTag('G', 'R', 'A', 'D');
constexpr SkFourByteTag kItalTag = SkSetFourByteTag('i', 't', 'a', 'l');
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kSlntTag = SkSetFourByteTag('s', 'l', 'n', 't');
constexpr SkFourByteTag kWdthTag = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

FontCustomPlatformData* LoadCustomPlatformData(const String& path) {
  std::optional<Vector<char>> font_data =
      test::ReadFromFile(test::PlatformTestDataPath(path));
  CHECK(font_data.has_value());
  scoped_refptr<SharedBuffer> buffer =
      SharedBuffer::Create(std::move(*font_data));
  String ots_parse_message;
  FontCustomPlatformData* result =
      FontCustomPlatformData::Create(buffer.get(), ots_parse_message);
  CHECK(result) << ots_parse_message;
  return result;
}

FontSelectionCapabilities AutoCapabilities() {
  return FontSelectionCapabilities(
      {kNormalWidthValue, kNormalWidthValue, FontSelectionRange::kSetFromAuto},
      {kNormalSlopeValue, kNormalSlopeValue, FontSelectionRange::kSetFromAuto},
      {kNormalWeightValue, kNormalWeightValue,
       FontSelectionRange::kSetFromAuto});
}

scoped_refptr<FontVariationSettings> MakeVariationSettings(
    std::initializer_list<FontVariationAxis> axes) {
  scoped_refptr<FontVariationSettings> settings =
      FontVariationSettings::Create();
  for (const FontVariationAxis& axis : axes) {
    settings->Append(axis);
  }
  return settings;
}

TEST(FontCustomPlatformDataTest, ClampToTypefaceModeClampsToTypefaceFvar) {
  // `roboto-ift.ttf`'s base `fvar` table caps `wght` at 400 and `wdth` at 100,
  // and has no `ital` axis.
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetSpecifiedSize(20);
  description.SetComputedSize(20);
  description.SetWeight(kBoldWeightValue);           // 700, above fvar max 400
  description.SetStretch(kExtraExpandedWidthValue);  // 150, above fvar max 100
  description.SetStyle(kItalicSlopeValue);           // 14
  description.SetStyleSyntax(FontDescription::StyleSyntax::kItalicKeyword);
  description.SetFontOpticalSizing(OpticalSizing::kAuto);

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kClampToTypeface);

  EXPECT_THAT(axes,
              testing::UnorderedElementsAre(FontVariationAxis(kWghtTag, 400),
                                            FontVariationAxis(kWdthTag, 100),
                                            FontVariationAxis(kSlntTag, -14),
                                            FontVariationAxis(kOpszTag, 20)));
}

TEST(FontCustomPlatformDataTest, IgnoreTypefaceAxesModePreservesUnclampedAxes) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetSpecifiedSize(20);
  description.SetComputedSize(20);
  description.SetWeight(kBoldWeightValue);           // 700, above fvar max 400
  description.SetStretch(kExtraExpandedWidthValue);  // 150, above fvar max 100
  description.SetStyle(kItalicSlopeValue);           // 14
  description.SetStyleSyntax(FontDescription::StyleSyntax::kItalicKeyword);
  description.SetFontOpticalSizing(OpticalSizing::kAuto);

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes,
              testing::UnorderedElementsAre(FontVariationAxis(kWghtTag, 700),
                                            FontVariationAxis(kWdthTag, 150),
                                            FontVariationAxis(kItalTag, 1),
                                            FontVariationAxis(kSlntTag, -14),
                                            FontVariationAxis(kOpszTag, 20)));
}

TEST(FontCustomPlatformDataTest, StaticFontWithClampToTypefaceReturnsEmpty) {
  FontCustomPlatformData* static_font = LoadCustomPlatformData("Ahem.woff");
  FontDescription description;
  description.SetWeight(kBoldWeightValue);

  Vector<FontVariationAxis, 5> axes = static_font->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kClampToTypeface);

  EXPECT_THAT(axes, testing::IsEmpty());
}

TEST(FontCustomPlatformDataTest, StaticFontWithIgnoreTypefaceAxesResolvesAxes) {
  FontCustomPlatformData* static_font = LoadCustomPlatformData("Ahem.woff");
  FontDescription description;
  description.SetWeight(kBoldWeightValue);

  Vector<FontVariationAxis, 5> axes = static_font->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes, testing::Contains(FontVariationAxis(kWghtTag, 700)));
}

TEST(FontCustomPlatformDataTest, ExplicitCapabilitiesClampAxes) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  const FontSelectionCapabilities explicit_capabilities(
      {FontSelectionValue(80), FontSelectionValue(120),
       FontSelectionRange::kSetExplicitly},
      {FontSelectionValue(0), FontSelectionValue(8),
       FontSelectionRange::kSetExplicitly},
      {FontSelectionValue(100), FontSelectionValue(400),
       FontSelectionRange::kSetExplicitly});
  FontDescription description;
  description.SetWeight(kBoldWeightValue);            // 700 -> clamped to 400
  description.SetStretch(kUltraCondensedWidthValue);  // 50 -> clamped to 80
  description.SetStyle(FontSelectionValue(15));       // 15 -> clamped to 8

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, explicit_capabilities,
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes, testing::IsSupersetOf({FontVariationAxis(kWghtTag, 400),
                                           FontVariationAxis(kWdthTag, 80),
                                           FontVariationAxis(kSlntTag, -8)}));
}

TEST(FontCustomPlatformDataTest,
     OpticalSizingNoneWithIgnoreTypefaceAxesOmitsOpsz) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetSpecifiedSize(20);
  description.SetComputedSize(20);
  description.SetFontOpticalSizing(OpticalSizing::kNone);

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes, testing::Not(testing::Contains(
                        testing::Property(&FontVariationAxis::Tag, kOpszTag))));
}

TEST(FontCustomPlatformDataTest, ZeroSizeWithSizeAdjustResolvesOpszToZero) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription zero_size;
  zero_size.SetSpecifiedSize(0);
  zero_size.SetComputedSize(0);
  FontDescription adjusted = zero_size.SizeAdjustedFontDescription(0.5f);
  adjusted.SetFontOpticalSizing(OpticalSizing::kAuto);

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      adjusted, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes, testing::Contains(FontVariationAxis(kOpszTag, 0)));
}

TEST(FontCustomPlatformDataTest, DisabledItalicAxisFeatureOmitsItal) {
  ScopedFontVariableItalicAxisForTest disable_ital(false);
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetStyle(kItalicSlopeValue);
  description.SetStyleSyntax(FontDescription::StyleSyntax::kItalicKeyword);

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(axes, testing::Not(testing::Contains(
                        testing::Property(&FontVariationAxis::Tag, kItalTag))));
}

TEST(FontCustomPlatformDataTest, VariationSettingsOverrideBuiltInAxes) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetWeight(kBoldWeightValue);
  description.SetVariationSettings(MakeVariationSettings(
      {{kGradTag, -50}, {kWghtTag, 250}, {kWghtTag, 350}}));

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(
      axes,
      testing::UnorderedElementsAre(
          FontVariationAxis(kWghtTag, 350), FontVariationAxis(kWdthTag, 100),
          FontVariationAxis(kItalTag, 0), FontVariationAxis(kSlntTag, 0),
          FontVariationAxis(kOpszTag, 0), FontVariationAxis(kGradTag, -50)));
}

TEST(FontCustomPlatformDataTest,
     NonMonotonicDuplicateCustomVariationSettingsKeepLastValue) {
  FontCustomPlatformData* platform_data =
      LoadCustomPlatformData("roboto-ift.ttf");
  FontDescription description;
  description.SetVariationSettings(
      MakeVariationSettings({{kGradTag, -50}, {kAaaaTag, 1}, {kGradTag, -25}}));

  Vector<FontVariationAxis, 5> axes = platform_data->ResolveVariationAxes(
      description, AutoCapabilities(),
      FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes);

  EXPECT_THAT(
      axes,
      testing::UnorderedElementsAre(
          FontVariationAxis(kWghtTag, 400), FontVariationAxis(kWdthTag, 100),
          FontVariationAxis(kItalTag, 0), FontVariationAxis(kSlntTag, 0),
          FontVariationAxis(kOpszTag, 0), FontVariationAxis(kGradTag, -25),
          FontVariationAxis(kAaaaTag, 1)));
}

}  // namespace
}  // namespace blink
