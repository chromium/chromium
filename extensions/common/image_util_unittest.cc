// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/image_util.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/logging_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

namespace extensions {

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

TEST(ImageUtilTest, IconTooLargeForAnalysis) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir));
  // This is a large icon which is entirely black, so it would be
  // visible. However, it exceeds the max allowed size for analysis,
  // so it will fail.
  base::FilePath icon_path = test_dir.AppendASCII("3000x3000.png");
  SkBitmap large_icon;
  SkBitmap rendered_icon;
  ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &large_icon));
  EXPECT_FALSE(image_util::RenderIconForVisibilityAnalysis(
      large_icon, SK_ColorWHITE, &rendered_icon));

  // Shrink the icon so it's under the limit. It should be visible.
  const SkImageInfo& image_info = large_icon.info();
  SkImageInfo new_image_info = SkImageInfo::Make(
      128, 128, image_info.colorType(), image_info.alphaType());
  ASSERT_TRUE(large_icon.setInfo(new_image_info));
  EXPECT_TRUE(image_util::RenderIconForVisibilityAnalysis(
      large_icon, SK_ColorWHITE, &rendered_icon));
  EXPECT_FALSE(rendered_icon.empty());
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
  DCHECK(image_util::RenderIconForVisibilityAnalysis(icon, background_color,
                                                     &bitmap));
  std::vector<unsigned char> output_data;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output_data));
  ASSERT_TRUE(base::WriteFile(rendered_icon_path, output_data));
}

}  // namespace

TEST(ImageUtilTest, DISABLED_AnalyzeAllDownloadedIcons) {
  // See the README in extensions/test/data/icon_visibility for more details
  // on running this test.
  // TODO(crbug.com/40559794): Remove this test when the bug is closed.
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

  const std::vector<std::string_view> urls = base::SplitStringPiece(
      file_data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string_view url : urls) {
    const std::string file_name = GURL(url).ExtractFileName();
    base::FilePath icon_path = downloaded_icons_path.AppendASCII(file_name);
    SkBitmap current_icon;
    ASSERT_TRUE(image_util::LoadPngFromFile(icon_path, &current_icon));
    if (!image_util::IsRenderedIconSufficientlyVisible(current_icon,
                                                       SK_ColorWHITE)) {
      output_file.WriteAtCurrentPos(base::as_byte_span(url));
      output_file.WriteAtCurrentPos(base::byte_span_from_cstring("\n"));
      WriteRenderedIcon(current_icon, SK_ColorWHITE,
                        rendered_icon_path.AppendASCII(file_name + ".png"));
    }
  }
}

}  // namespace extensions
