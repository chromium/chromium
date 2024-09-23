// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/palette_interpolation.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#include <utility>
#include <vector>

namespace {

constexpr double kMaxAlphaDifference = 0.01;

String pathToColorPalettesTestFont() {
  base::FilePath wpt_palette_font_path(
      blink::StringToFilePath(blink::test::BlinkWebTestsDir()));
  wpt_palette_font_path = wpt_palette_font_path.Append(FILE_PATH_LITERAL(
      "external/wpt/css/css-fonts/resources/COLR-palettes-test-font.ttf"));
  return blink::FilePathToString(wpt_palette_font_path);
}
String pathToNonColorTestFont() {
  return blink::test::BlinkWebTestsFontsTestDataPath("Ahem.ttf");
}

}  // namespace

namespace blink {

class PaletteInterpolationTest : public FontTestBase {
 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;

    Font color_palette_font = blink::test::CreateTestFont(
        AtomicString("Ahem"), pathToColorPalettesTestFont(), 16, &ligatures);
    color_palette_typeface_ =
        sk_ref_sp(color_palette_font.PrimaryFont()->PlatformData().Typeface());

    Font non_color_font = blink::test::CreateTestFont(
        AtomicString("Ahem"), pathToNonColorTestFont(), 16, &ligatures);
    non_color_ahem_typeface_ =
        sk_ref_sp(non_color_font.PrimaryFont()->PlatformData().Typeface());
  }

  void ExpectColorsEqualInSRGB(
      Vector<FontPalette::FontPaletteOverride> overrides1,
      Vector<FontPalette::FontPaletteOverride> overrides2) {
    EXPECT_EQ(overrides1.size(), overrides2.size());
    for (wtf_size_t i = 0; i < overrides1.size(); i++) {
      EXPECT_EQ(overrides1[i].index, overrides2[i].index);
      Color color1 = overrides1[i].color;
      Color color2 = overrides2[i].color;
      EXPECT_EQ(DifferenceSquared(color1, color2), 0);
      // Due to the conversion from oklab to SRGB we should use epsilon
      // comparison.
      EXPECT_TRUE(std::fabs(color1.Alpha() - color2.Alpha()) <
                  kMaxAlphaDifference);
    }
  }

  sk_sp<SkTypeface> color_palette_typeface_;
  sk_sp<SkTypeface> non_color_ahem_typeface_;
};

TEST_F(PaletteInterpolationTest, RetrievePaletteIndexFromColorFont) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette =
      FontPalette::Create(FontPalette::kDarkPalette);
  std::optional<uint16_t> index =
      palette_interpolation.RetrievePaletteIndex(palette.get());
  EXPECT_TRUE(index.has_value());
  EXPECT_EQ(*index, 3);
}

TEST_F(PaletteInterpolationTest, RetrievePaletteIndexFromNonColorFont) {
  PaletteInterpolation palette_interpolation(non_color_ahem_typeface_);
  scoped_refptr<FontPalette> palette =
      FontPalette::Create(FontPalette::kLightPalette);
  std::optional<uint16_t> index =
      palette_interpolation.RetrievePaletteIndex(palette.get());
  EXPECT_FALSE(index.has_value());
}

TEST_F(PaletteInterpolationTest, MixCustomPalettesAtHalfTime) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Create(AtomicString("palette1"));
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

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Create(AtomicString("palette2"));
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
      FontPalette::Mix(palette_start, palette_end, 50, 50, 0.5, 1.0,
                       Color::ColorSpace::kOklab, std::nullopt);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be half-way between palette_start and palette_end
  // after interpolation in the Oklab interpolation color space and conversion
  // back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, Color::FromRGBA(254, 255, 172, 255)},
      {1, Color::FromRGBA(0, 0, 99, 255)},
      {2, Color::FromRGBA(253, 45, 155, 255)},
      {3, Color::FromRGBA(0, 255, 169, 255)},
      {4, Color::FromRGBA(254, 255, 172, 255)},
      {5, Color::FromRGBA(0, 0, 99, 255)},
      {6, Color::FromRGBA(253, 45, 155, 255)},
      {7, Color::FromRGBA(0, 255, 169, 255)},
  };
  ExpectColorsEqualInSRGB(actual_color_records, expected_color_records);
}

TEST_F(PaletteInterpolationTest, MixCustomAndNonExistingPalettes) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Create(AtomicString("palette1"));
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

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Create(AtomicString("palette2"));
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 16});
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

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 50, 50, 0.5, 1.0,
                       Color::ColorSpace::kOklab, std::nullopt);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be half-way between palette_start and normal
  // palette after interpolation in the Oklab interpolation color space and
  // conversion back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, Color::FromRGBA(99, 99, 0, 255)},
      {1, Color::FromRGBA(140, 83, 162, 255)},
      {2, Color::FromRGBA(198, 180, 180, 255)},
      {3, Color::FromRGBA(176, 255, 176, 255)},
      {4, Color::FromRGBA(116, 163, 255, 255)},
      {5, Color::FromRGBA(99, 0, 99, 255)},
      {6, Color::FromRGBA(210, 169, 148, 255)},
      {7, Color::FromRGBA(173, 255, 166, 255)},
  };
  ExpectColorsEqualInSRGB(actual_color_records, expected_color_records);
}

TEST_F(PaletteInterpolationTest, MixNonExistingPalettes) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Create(AtomicString("palette1"));
  // Palette under index 16 does not exist, so instead normal palette is used.
  palette_start->SetBasePalette({FontPalette::kIndexBasePalette, 16});

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Create(AtomicString("palette2"));
  // Palette under index 17 does not exist, so instead normal palette is used.
  palette_end->SetBasePalette({FontPalette::kIndexBasePalette, 17});

  scoped_refptr<FontPalette> palette =
      FontPalette::Mix(palette_start, palette_end, 50, 50, 0.5, 1.0,
                       Color::ColorSpace::kOklab, std::nullopt);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // Since both of the endpoints are equal and have color records from normal
  // palette, we expect each colors from the normal palette in the result list.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, Color::FromRGBA(0, 0, 0, 255)},
      {1, Color::FromRGBA(255, 0, 0, 255)},
      {2, Color::FromRGBA(0, 255, 0, 255)},
      {3, Color::FromRGBA(255, 255, 0, 255)},
      {4, Color::FromRGBA(0, 0, 255, 255)},
      {5, Color::FromRGBA(255, 0, 255, 255)},
      {6, Color::FromRGBA(0, 255, 255, 255)},
      {7, Color::FromRGBA(255, 255, 255, 255)},
  };
  ExpectColorsEqualInSRGB(actual_color_records, expected_color_records);
}

TEST_F(PaletteInterpolationTest, MixCustomPalettesInOklab) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Create(AtomicString("palette1"));
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

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Create(AtomicString("palette2"));
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
      FontPalette::Mix(palette_start, palette_end, 70, 30, 0.3, 1.0,
                       Color::ColorSpace::kOklab, std::nullopt);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be equal palette_start * 0.7 + palette_end * 0.3
  // after interpolation in the sRGB interpolation color space.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, Color::FromRGBA(254, 255, 131, 255)},
      {1, Color::FromRGBA(0, 0, 158, 255)},
      {2, Color::FromRGBA(254, 42, 196, 255)},
      {3, Color::FromRGBA(0, 255, 205, 255)},
      {4, Color::FromRGBA(254, 255, 207, 255)},
      {5, Color::FromRGBA(0, 0, 46, 255)},
      {6, Color::FromRGBA(254, 39, 112, 255)},
      {7, Color::FromRGBA(0, 255, 128, 255)},
  };
  ExpectColorsEqualInSRGB(actual_color_records, expected_color_records);
}

TEST_F(PaletteInterpolationTest, MixCustomPalettesInSRGB) {
  PaletteInterpolation palette_interpolation(color_palette_typeface_);
  scoped_refptr<FontPalette> palette_start =
      FontPalette::Create(AtomicString("palette1"));
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

  scoped_refptr<FontPalette> palette_end =
      FontPalette::Create(AtomicString("palette2"));
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
      FontPalette::Mix(palette_start, palette_end, 70, 30, 0.3, 1.0,
                       Color::ColorSpace::kSRGB, std::nullopt);
  Vector<FontPalette::FontPaletteOverride> actual_color_records =
      palette_interpolation.ComputeInterpolableFontPalette(palette.get());
  // We expect each color to be equal palette_start * 0.7 + palette_end * 0.3
  // after interpolation in the Oklab interpolation color space and conversion
  // back to sRGB.
  Vector<FontPalette::FontPaletteOverride> expected_color_records = {
      {0, Color::FromRGBA(255, 255, 77, 255)},
      {1, Color::FromRGBA(0, 0, 179, 255)},
      {2, Color::FromRGBA(255, 0, 179, 255)},
      {3, Color::FromRGBA(0, 255, 179, 255)},
      {4, Color::FromRGBA(255, 255, 179, 255)},
      {5, Color::FromRGBA(0, 0, 77, 255)},
      {6, Color::FromRGBA(255, 0, 77, 255)},
      {7, Color::FromRGBA(0, 255, 77, 255)},
  };
  ExpectColorsEqualInSRGB(actual_color_records, expected_color_records);
}

}  // namespace blink
