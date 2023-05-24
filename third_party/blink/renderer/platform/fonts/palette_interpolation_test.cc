// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/palette_interpolation.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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

class PaletteInterpolationTest : public FontTestBase {
 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;

    Font colr_palette_font = blink::test::CreateTestFont(
        "Ahem", pathToColrPalettesTestFont(), 16, &ligatures);
    colr_palette_typeface_ =
        sk_ref_sp(colr_palette_font.PrimaryFont()->PlatformData().Typeface());

    Font non_colr_font = blink::test::CreateTestFont(
        "Ahem", pathToNonColrTestFont(), 16, &ligatures);
    non_colr_ahem_typeface_ =
        sk_ref_sp(non_colr_font.PrimaryFont()->PlatformData().Typeface());
  }

  sk_sp<SkTypeface> colr_palette_typeface_;
  sk_sp<SkTypeface> non_colr_ahem_typeface_;
};

TEST_F(PaletteInterpolationTest, RetrievePaletteIndexFromColorFont) {
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette =
      FontPalette::Create(FontPalette::kDarkPalette);
  absl::optional<uint16_t> index =
      palette_interpolation.RetrievePaletteIndex(palette.get());
  EXPECT_TRUE(index.has_value());
  EXPECT_EQ(*index, 3);
}

TEST_F(PaletteInterpolationTest, RetrievePaletteIndexFromNonColorFont) {
  PaletteInterpolation palette_interpolation(non_colr_ahem_typeface_);
  scoped_refptr<FontPalette> palette =
      FontPalette::Create(FontPalette::kLightPalette);
  absl::optional<uint16_t> index =
      palette_interpolation.RetrievePaletteIndex(palette.get());
  EXPECT_FALSE(index.has_value());
}

TEST_F(PaletteInterpolationTest, MixCustomPalettesAtHalfTime) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create("palette1");
  palette_start->SetBasePalette({FontPalette::kIndexBasePalette, 3});
  // palette_start has the following list of color records:
  // { rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%) }

  scoped_refptr<FontPalette> palette_end = FontPalette::Create("palette2");
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 7});
  // palette_end has the following list of color records:
  // { rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%),
  //   rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%) }

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.5);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be half-way between palette_start and palette_end
  // after interpolation in the Oklab interpolation color space and conversion
  // back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, SkColorSetARGB(255, 254, 255, 172)},
      {1, SkColorSetARGB(255, 0, 0, 99)},
      {2, SkColorSetARGB(255, 253, 45, 155)},
      {3, SkColorSetARGB(255, 0, 255, 169)},
      {4, SkColorSetARGB(255, 254, 255, 172)},
      {5, SkColorSetARGB(255, 0, 0, 99)},
      {6, SkColorSetARGB(255, 253, 45, 155)},
      {7, SkColorSetARGB(255, 0, 255, 169)},
  };
  EXPECT_EQ(expected_color_records, actual_color_records);
}

TEST_F(PaletteInterpolationTest, MixCustomAndNonExistingPalettes) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create("palette1");
  palette_start->SetBasePalette({FontPalette::kIndexBasePalette, 3});
  // palette_start has the following list of color records:
  // { rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%) }

  scoped_refptr<FontPalette> palette_end = FontPalette::Create("palette2");
  // Palette under index 16 does not exist, so instead normal palette is used.
  // Normal palette has the following list of color records:
  // { rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%),
  //   rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%) }
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 16});

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.5);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be half-way between palette_start and normal
  // palette after interpolation in the Oklab interpolation color space and
  // conversion back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, SkColorSetARGB(255, 99, 99, 0)},
      {1, SkColorSetARGB(255, 140, 83, 162)},
      {2, SkColorSetARGB(255, 198, 180, 180)},
      {3, SkColorSetARGB(255, 176, 255, 176)},
      {4, SkColorSetARGB(255, 116, 163, 255)},
      {5, SkColorSetARGB(255, 99, 0, 99)},
      {6, SkColorSetARGB(255, 210, 169, 148)},
      {7, SkColorSetARGB(255, 173, 255, 166)},
  };
  EXPECT_EQ(expected_color_records, actual_color_records);
}

TEST_F(PaletteInterpolationTest, MixNonExistingPalettes) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create("palette1");
  // Palette under index 16 does not exist, so instead normal palette is used.
  palette_start->SetBasePalette({FontPalette::kIndexBasePalette, 16});

  scoped_refptr<FontPalette> palette_end = FontPalette::Create("palette2");
  // Palette under index 17 does not exist, so instead normal palette is used.
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 17});

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.5);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // Since both of the endpoints are equal and have color records from normal
  // palette, we expect each colors from the normal palette in the result list.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, SkColorSetARGB(255, 0, 0, 0)},
      {1, SkColorSetARGB(255, 255, 0, 0)},
      {2, SkColorSetARGB(255, 0, 255, 0)},
      {3, SkColorSetARGB(255, 255, 255, 0)},
      {4, SkColorSetARGB(255, 0, 0, 255)},
      {5, SkColorSetARGB(255, 255, 0, 255)},
      {6, SkColorSetARGB(255, 0, 255, 255)},
      {7, SkColorSetARGB(255, 255, 255, 255)},
  };
  EXPECT_EQ(expected_color_records, actual_color_records);
}

TEST_F(PaletteInterpolationTest, MixScaledAndAddedPalettes) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Add(FontPalette::Create(FontPalette::kDarkPalette),
                       FontPalette::Create(FontPalette::kLightPalette));
  // palette_start has the following list of color records:
  // { rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%) }

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Scale(FontPalette::Create(), 0.1);
  // palette_end has the following list of color records:
  // { rgba(0, 0, 0, 26) = oklab(0%, 0%, 0% / 0.1),
  //   rgba(26, 0, 0, 26) = oklab(62.8%, 56.25%, 31.5% / 0.1),
  //   rgba(0, 26, 0, 26) = oklab(86.6%, -58.5%, 44.75% / 0.1),
  //   rgba(26, 26, 0, 26) = oklab(96.8%, -17.75%, 49.75% / 0.1),
  //   rgba(0, 0, 26, 26) = oklab(45.2%, -8%, -78% / 0.1),
  //   rgba(26, 0, 26, 26) = oklab(70.2%, 68.75%, -42.25% / 0.1),
  //   rgba(0, 26, 26, 26) = oklab(90.5%, -37.25%, -9.75% / 0.1),
  //   rgba(26, 26, 26, 26) = oklab(100%, 0%, 0% / 0.1) }

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.5);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be half-way between palette_start and palette_end
  // after interpolation in the Oklab interpolation color space and conversion
  // back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, SkColorSetARGB(141, 122, 224, 0)},
      {1, SkColorSetARGB(141, 111, 226, 225)},
      {2, SkColorSetARGB(141, 226, 36, 226)},
      {3, SkColorSetARGB(141, 227, 187, 226)},
      {4, SkColorSetARGB(141, 72, 226, 227)},
      {5, SkColorSetARGB(141, 228, 226, 227)},
      {6, SkColorSetARGB(141, 227, 15, 7)},
      {7, SkColorSetARGB(141, 227, 214, 15)},
  };
  EXPECT_EQ(expected_color_records, actual_color_records);
}

TEST_F(PaletteInterpolationTest, MixCustomPalettes) {
  ScopedFontPaletteAnimationForTest scoped_feature(true);
  PaletteInterpolation palette_interpolation(colr_palette_typeface_);
  scoped_refptr<FontPalette> palette_start = FontPalette::Create("palette1");
  palette_start->SetBasePalette({FontPalette::kIndexBasePalette, 3});
  // palette_start has the following list of color records:
  // { rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%),
  //   rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%) }

  scoped_refptr<FontPalette> palette_end = FontPalette::Create("palette2");
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 7});
  // palette_end has the following list of color records:
  // { rgba(255, 255, 255, 255) = oklab(100%, 0%, 0%),
  //   rgba(0, 0, 0, 255) = oklab(0%, 0%, 0%),
  //   rgba(255, 0, 0, 255) = oklab(62.8%, 56.25%, 31.5%),
  //   rgba(0, 255, 0, 255) = oklab(86.6%, -58.5%, 44.75%),
  //   rgba(255, 255, 0, 255) = oklab(96.8%, -17.75%, 49.75%),
  //   rgba(0, 0, 255, 255) = oklab(45.2%, -8%, -78%),
  //   rgba(255, 0, 255, 255) = oklab(70.2%, 68.75%, -42.25%),
  //   rgba(0, 255, 255, 255) = oklab(90.5%, -37.25%, -9.75%) }

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 0.3);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be equal palette_start * 0.7 + palette_end * 0.3
  // after interpolation in the Oklab interpolation color space and conversion
  // back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, SkColorSetARGB(255, 254, 255, 131)},
      {1, SkColorSetARGB(255, 0, 0, 158)},
      {2, SkColorSetARGB(255, 254, 42, 196)},
      {3, SkColorSetARGB(255, 0, 255, 205)},
      {4, SkColorSetARGB(255, 254, 255, 207)},
      {5, SkColorSetARGB(255, 0, 0, 46)},
      {6, SkColorSetARGB(255, 254, 39, 112)},
      {7, SkColorSetARGB(255, 0, 255, 128)},
  };
  EXPECT_EQ(expected_color_records, actual_color_records);
}

}  // namespace blink
