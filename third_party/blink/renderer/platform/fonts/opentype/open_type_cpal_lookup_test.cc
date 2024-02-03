// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#include <utility>
#include <vector>

namespace {
String pathToColrPalettesTestFont() {
  base::FilePath wpt_palette_font_path(
      blink::StringToFilePath(blink::test::BlinkWebTestsDir()));
  wpt_palette_font_path = wpt_palette_font_path.Append(FILE_PATH_LITERAL(
      "external/wpt/css/css-fonts/resources/COLR-palettes-test-font.ttf"));
  return blink::FilePathToString(wpt_palette_font_path);
}
String pathToNonColrTestFont() {
  return blink::test::BlinkWebTestsFontsTestDataPath("Ahem.ttf");
}
}  // namespace

namespace blink {

class OpenTypeCpalLookupTest : public FontTestBase {
 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;

    Font colr_palette_font = blink::test::CreateTestFont(
        AtomicString("Ahem"), pathToColrPalettesTestFont(), 16, &ligatures);
    colr_palette_typeface_ =
        sk_ref_sp(colr_palette_font.PrimaryFont()->PlatformData().Typeface());

    Font non_colr_font = blink::test::CreateTestFont(
        AtomicString("Ahem"), pathToNonColrTestFont(), 16, &ligatures);
    non_colr_ahem_typeface_ =
        sk_ref_sp(non_colr_font.PrimaryFont()->PlatformData().Typeface());
  }

  sk_sp<SkTypeface> colr_palette_typeface_;
  sk_sp<SkTypeface> non_colr_ahem_typeface_;
};

TEST_F(OpenTypeCpalLookupTest, NoResultForNonColr) {
  for (auto& palette_use : {OpenTypeCpalLookup::kUsableWithLightBackground,
                            OpenTypeCpalLookup::kUsableWithDarkBackground}) {
    std::optional<uint16_t> palette_result =
        OpenTypeCpalLookup::FirstThemedPalette(non_colr_ahem_typeface_,
                                               palette_use);
    EXPECT_FALSE(palette_result.has_value());
  }
}

TEST_F(OpenTypeCpalLookupTest, DarkLightPalettes) {
  // COLR-palettes-test-font.tff dumped with FontTools has
  //     <palette index="2" type="1">[...]
  //     <palette index="3" type="2">
  // meaning palette index 2 is the first palette usable for light backgrounds,
  // and palette index 3 is the first palette usable for dark background.
  std::vector<std::pair<OpenTypeCpalLookup::PaletteUse, uint16_t>> expectations{
      {OpenTypeCpalLookup::kUsableWithLightBackground, 2},
      {OpenTypeCpalLookup::kUsableWithDarkBackground, 3}};
  for (auto& expectation : expectations) {
    std::optional<uint16_t> palette_result =
        OpenTypeCpalLookup::FirstThemedPalette(colr_palette_typeface_,
                                               expectation.first);
    EXPECT_TRUE(palette_result.has_value());
    EXPECT_EQ(*palette_result, expectation.second);
  }
}

TEST_F(OpenTypeCpalLookupTest, RetrieveColorRecordsFromExistingPalette) {
  Vector<Color> expected_color_records = {
      Color::FromRGBA(255, 255, 0, 255),   Color::FromRGBA(0, 0, 255, 255),
      Color::FromRGBA(255, 0, 255, 255),   Color::FromRGBA(0, 255, 255, 255),
      Color::FromRGBA(255, 255, 255, 255), Color::FromRGBA(0, 0, 0, 255),
      Color::FromRGBA(255, 0, 0, 255),     Color::FromRGBA(0, 255, 0, 255),
  };

  Vector<Color> actual_color_records =
      OpenTypeCpalLookup::RetrieveColorRecords(colr_palette_typeface_, 3);

  EXPECT_EQ(expected_color_records, actual_color_records);
}

TEST_F(OpenTypeCpalLookupTest, RetrieveColorRecordsFromNonExistingPalette) {
  // Palette at index 16 does not exist in the font should return empty Vector
  Vector<Color> actual_color_records =
      OpenTypeCpalLookup::RetrieveColorRecords(colr_palette_typeface_, 16);

  EXPECT_EQ(actual_color_records.size(), 0u);
}

}  // namespace blink
