// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/image_util.h"
#include "extensions/test/logging_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

namespace extensions {

void RunPassHexTest(const std::string& css_string, SkColor expected_result) {
  SkColor color = 0;
  EXPECT_TRUE(image_util::ParseHexColorString(css_string, &color));
  EXPECT_EQ(color, expected_result);
}

void RunFailHexTest(const std::string& css_string) {
  SkColor color = 0;
  EXPECT_FALSE(image_util::ParseHexColorString(css_string, &color));
}

void RunPassHslTest(const std::string& hsl_string, SkColor expected) {
  SkColor color = 0;
  EXPECT_TRUE(image_util::ParseCssColorString(hsl_string, &color));
  EXPECT_EQ(color, expected);
}

void RunFailHslTest(const std::string& hsl_string) {
  SkColor color = 0;
  EXPECT_FALSE(image_util::ParseCssColorString(hsl_string, &color));
}

void RunPassRgbTest(const std::string& rgb_string, SkColor expected) {
  SkColor color = 0;
  EXPECT_TRUE(image_util::ParseRgbColorString(rgb_string, &color));
  EXPECT_EQ(color, expected);
}

void RunFailRgbTest(const std::string& rgb_string) {
  SkColor color = 0;
  EXPECT_FALSE(image_util::ParseRgbColorString(rgb_string, &color));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundNormalCSS) {
  RunPassHexTest("#34006A", SkColorSetARGB(0xFF, 0x34, 0, 0x6A));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundShortCSS) {
  RunPassHexTest("#A1E", SkColorSetARGB(0xFF, 0xAA, 0x11, 0xEE));
}

TEST(ImageUtilTest, ParseHexWithAlphaCSS) {
  RunPassHexTest("#340061CC", SkColorSetARGB(0xCC, 0x34, 0, 0x61));
}

TEST(ImageUtilTest, ParseHexWithAlphaShortCSS) {
  RunPassHexTest("#A1E9", SkColorSetARGB(0x99, 0xAA, 0x11, 0xEE));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSNoHash) {
  RunFailHexTest("11FF22");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSTooShort) {
  RunFailHexTest("#FF22C");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSTooLong) {
  RunFailHexTest("#FF22128");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSInvalid) {
  RunFailHexTest("#-22128");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSInvalidWithPlus) {
  RunFailHexTest("#+22128");
}

TEST(ImageUtilTest, AcceptHsl) {
  // Run basic color tests.
  RunPassHslTest("hsl(0, 100%, 50%)", SK_ColorRED);
  RunPassHslTest("hsl(120, 100%, 50%)", SK_ColorGREEN);
  RunPassHslTest("hsl(240, 100%, 50%)", SK_ColorBLUE);
  RunPassHslTest("hsl(180, 100%, 50%)", SK_ColorCYAN);

  // Passing in >100% saturation should be equivalent to 100%.
  RunPassHslTest("hsl(120, 200%, 50%)", SK_ColorGREEN);

  // Passing in the same degree +/- full rotations should be equivalent.
  RunPassHslTest("hsl(480, 100%, 50%)", SK_ColorGREEN);
  RunPassHslTest("hsl(-240, 100%, 50%)", SK_ColorGREEN);

  // We should be able to parse doubles
  RunPassHslTest("hsl(120.0, 100.0%, 50.0%)", SK_ColorGREEN);
}

TEST(ImageUtilTest, InvalidHsl) {
  RunFailHslTest("(0,100%,50%)");
  RunFailHslTest("[0, 100, 50]");
  RunFailHslTest("hs l(0,100%,50%)");
  RunFailHslTest("rgb(0,100%,50%)");
  RunFailHslTest("hsl(0,100%)");
  RunFailHslTest("hsl(100%,50%)");
  RunFailHslTest("hsl(120, 100, 50)");
  RunFailHslTest("hsl[120, 100%, 50%]");
  RunFailHslTest("hsl(120, 100%, 50%, 1.0)");
  RunFailHslTest("hsla(120, 100%, 50%)");
}

TEST(ImageUtilTest, AcceptHsla) {
  // Run basic color tests.
  RunPassHslTest("hsla(0, 100%, 50%, 1.0)", SK_ColorRED);
  RunPassHslTest("hsla(0, 100%, 50%, 0.0)",
                 SkColorSetARGB(0x00, 0xFF, 0x00, 0x00));
  RunPassHslTest("hsla(0, 100%, 50%, 0.5)",
                 SkColorSetARGB(0x7F, 0xFF, 0x00, 0x00));
  RunPassHslTest("hsla(0, 100%, 50%, 0.25)",
                 SkColorSetARGB(0x3F, 0xFF, 0x00, 0x00));
  RunPassHslTest("hsla(0, 100%, 50%, 0.75)",
                 SkColorSetARGB(0xBF, 0xFF, 0x00, 0x00));

  // We should able to parse integer alpha value.
  RunPassHslTest("hsla(0, 100%, 50%, 1)", SK_ColorRED);
}

TEST(ImageUtilTest, AcceptRgb) {
  // Run basic color tests.
  RunPassRgbTest("rgb(255,0,0)", SK_ColorRED);
  RunPassRgbTest("rgb(0,    255, 0)", SK_ColorGREEN);
  RunPassRgbTest("rgb(0, 0, 255)", SK_ColorBLUE);
}

TEST(ImageUtilTest, InvalidRgb) {
  RunFailRgbTest("(0,100,50)");
  RunFailRgbTest("[0, 100, 50]");
  RunFailRgbTest("rg b(0,100,50)");
  RunFailRgbTest("rgb(0,-100, 10)");
  RunFailRgbTest("rgb(100,50)");
  RunFailRgbTest("rgb(120.0, 100.6, 50.3)");
  RunFailRgbTest("rgb[120, 100, 50]");
  RunFailRgbTest("rgb(120, 100, 50, 1.0)");
  RunFailRgbTest("rgba(120, 100, 50)");
  RunFailRgbTest("rgb(0, 300, 0)");
  // This is valid RGB but we don't support percentages yet.
  RunFailRgbTest("rgb(100%, 0%, 100%)");
}

TEST(ImageUtilTest, AcceptRgba) {
  // Run basic color tests.
  RunPassRgbTest("rgba(255, 0, 0, 1.0)", SK_ColorRED);
  RunPassRgbTest("rgba(255, 0, 0, 0.0)",
                 SkColorSetARGB(0x00, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.5)",
                 SkColorSetARGB(0x7F, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.25)",
                 SkColorSetARGB(0x3F, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.75)",
                 SkColorSetARGB(0xBF, 0xFF, 0x00, 0x00));

  // We should able to parse an integer alpha value.
  RunPassRgbTest("rgba(255, 0, 0, 1)", SK_ColorRED);
}

TEST(ImageUtilTest, BasicColorKeyword) {
  SkColor color = 0;
  EXPECT_TRUE(image_util::ParseCssColorString("red", &color));
  EXPECT_EQ(color, SK_ColorRED);

  EXPECT_TRUE(image_util::ParseCssColorString("blue", &color));
  EXPECT_EQ(color, SK_ColorBLUE);

  EXPECT_FALSE(image_util::ParseCssColorString("my_red", &color));
}

TEST(ImageUtilTest, IsIconSufficientlyVisible) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir));
  base::FilePath icon_path;
  const std::string metric_name =
      "Extensions.IsRenderedIconSufficientlyVisibleTime";
  {
    base::HistogramTester histogram_tester;
    // This icon has all transparent pixels, so it will fail.
    icon_path = test_dir.AppendASCII("transparent_icon.png");
    SkBitmap transparent_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &transparent_icon));
    EXPECT_FALSE(image_util::IsIconSufficientlyVisible(transparent_icon));
    EXPECT_FALSE(image_util::IsRenderedIconSufficientlyVisible(transparent_icon,
                                                               SK_ColorWHITE));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with an icon that has one opaque pixel.
    icon_path = test_dir.AppendASCII("one_pixel_opaque_icon.png");
    SkBitmap visible_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &visible_icon));
    EXPECT_FALSE(image_util::IsIconSufficientlyVisible(visible_icon));
    EXPECT_FALSE(image_util::IsRenderedIconSufficientlyVisible(visible_icon,
                                                               SK_ColorWHITE));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with an icon that has one transparent pixel.
    icon_path = test_dir.AppendASCII("one_pixel_transparent_icon.png");
    SkBitmap visible_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &visible_icon));
    EXPECT_TRUE(image_util::IsIconSufficientlyVisible(visible_icon));
    EXPECT_TRUE(image_util::IsRenderedIconSufficientlyVisible(visible_icon,
                                                              SK_ColorWHITE));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with an icon that is completely opaque.
    icon_path = test_dir.AppendASCII("opaque_icon.png");
    SkBitmap visible_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &visible_icon));
    EXPECT_TRUE(image_util::IsIconSufficientlyVisible(visible_icon));
    EXPECT_TRUE(image_util::IsRenderedIconSufficientlyVisible(visible_icon,
                                                              SK_ColorWHITE));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with an icon that is rectangular.
    icon_path = test_dir.AppendASCII("rectangle.png");
    SkBitmap visible_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &visible_icon));
    EXPECT_TRUE(image_util::IsIconSufficientlyVisible(visible_icon));
    EXPECT_TRUE(image_util::IsRenderedIconSufficientlyVisible(visible_icon,
                                                              SK_ColorWHITE));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with a solid color icon that is completely opaque. Use the icon's
    // color as the background color in the call to analyze its visibility.
    // It should be invisible in this case.
    icon_path = test_dir.AppendASCII("grey_21x21.png");
    SkBitmap solid_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &solid_icon));
    const SkColor pixel_color = solid_icon.getColor(0, 0);
    EXPECT_FALSE(
        image_util::IsRenderedIconSufficientlyVisible(solid_icon, pixel_color));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
  {
    base::HistogramTester histogram_tester;
    // Test with a two-color icon that is completely opaque. Use one of the
    // icon's colors as the background color in the call to analyze its
    // visibility. It should be visible in this case.
    icon_path = test_dir.AppendASCII("two_color_21x21.png");
    SkBitmap two_color_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &two_color_icon));
    const SkColor pixel_color = two_color_icon.getColor(0, 0);
    EXPECT_TRUE(image_util::IsRenderedIconSufficientlyVisible(two_color_icon,
                                                              pixel_color));
    histogram_tester.ExpectTotalCount(metric_name, 1);
  }
}

TEST(ImageUtilTest, MANUAL_IsIconSufficientlyVisiblePerfTest) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir));
  base::FilePath icon_path;
  // This icon has all transparent pixels.
  icon_path = test_dir.AppendASCII("transparent_icon.png");
  SkBitmap invisible_icon;
  ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &invisible_icon));
  // This icon is completely opaque.
  icon_path = test_dir.AppendASCII("opaque_icon.png");
  SkBitmap visible_icon;
  ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &visible_icon));

  static constexpr char kInvisibleTimerId[] = "InvisibleIcon";
  static constexpr char kVisibleTimerId[] = "VisibleIcon";
  static constexpr char kInvisibleRenderedTimerId[] = "InvisibleRenderedIcon";
  static constexpr char kVisibleRenderedTimerId[] = "VisibleRenderedIcon";
  constexpr int kIterations = 100000;

  for (int i = 0; i < kIterations; ++i) {
    LoggingTimer timer(kInvisibleTimerId);
    EXPECT_FALSE(image_util::IsIconSufficientlyVisible(invisible_icon));
  }

  for (int i = 0; i < kIterations; ++i) {
    LoggingTimer timer(kVisibleTimerId);
    EXPECT_TRUE(image_util::IsIconSufficientlyVisible(visible_icon));
  }

  for (int i = 0; i < kIterations; ++i) {
    LoggingTimer timer(kInvisibleRenderedTimerId);
    EXPECT_FALSE(image_util::IsRenderedIconSufficientlyVisible(invisible_icon,
                                                               SK_ColorWHITE));
  }

  for (int i = 0; i < kIterations; ++i) {
    LoggingTimer timer(kVisibleRenderedTimerId);
    EXPECT_TRUE(image_util::IsRenderedIconSufficientlyVisible(visible_icon,
                                                              SK_ColorWHITE));
  }

  LoggingTimer::Print();
}

namespace {

void WriteRenderedIcon(const SkBitmap& icon,
                       SkColor background_color,
                       const base::FilePath& rendered_icon_path) {
  SkBitmap bitmap;
  image_util::RenderIconForVisibilityAnalysis(icon, background_color, &bitmap);
  std::vector<unsigned char> output_data;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output_data));
  const int bytes_to_write = output_data.size();
  ASSERT_EQ(bytes_to_write,
            base::WriteFile(rendered_icon_path,
                            reinterpret_cast<const char*>(&output_data[0]),
                            bytes_to_write));
}

}  // namespace

TEST(ImageUtilTest, DISABLED_AnalyzeAllDownloadedIcons) {
  // See the README in extensions/test/data/icon_visibility for more details
  // on running this test.
  // TODO(crbug.com/805600): Remove this test when the bug is closed.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir));
  test_dir = test_dir.AppendASCII("icon_visibility");
  base::FilePath icons_file_path = test_dir.AppendASCII("source_urls.txt");
  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(icons_file_path, &file_data));
  base::FilePath output_file_path =
      test_dir.AppendASCII("invisible_source_urls.txt");
  base::File output_file(output_file_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE);
  ASSERT_TRUE(output_file.IsValid());
  base::FilePath rendered_icon_path = test_dir.AppendASCII("rendered_pngs");
  ASSERT_TRUE(base::CreateDirectory(rendered_icon_path));

  base::FilePath downloaded_icons_path = test_dir.AppendASCII("pngs");
  ASSERT_TRUE(base::DirectoryExists(downloaded_icons_path));

  const std::vector<base::StringPiece> urls = base::SplitStringPiece(
      file_data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const base::StringPiece url : urls) {
    const std::string file_name = GURL(url).ExtractFileName();
    base::FilePath icon_path = downloaded_icons_path.AppendASCII(file_name);
    SkBitmap current_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &current_icon));
    if (!image_util::IsRenderedIconSufficientlyVisible(current_icon,
                                                       SK_ColorWHITE)) {
      output_file.WriteAtCurrentPos(url.data(), url.length());
      output_file.WriteAtCurrentPos("\n", 1);
      WriteRenderedIcon(current_icon, SK_ColorWHITE,
                        rendered_icon_path.AppendASCII(file_name + ".png"));
    }
  }
}

}  // namespace extensions
