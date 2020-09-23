// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/test/test_client.h"
#include "pdf/thumbnail.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

namespace {

TEST(PDFiumPageHelperTest, ToPDFiumRotation) {
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kOriginal), 0);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise90), 1);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise180), 2);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise270), 3);
}

TEST(PDFiumPageHelperDeathTest, ToPDFiumRotation) {
  PageOrientation invalid_orientation = static_cast<PageOrientation>(-1);
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH(ToPDFiumRotation(invalid_orientation));
#else
  EXPECT_EQ(ToPDFiumRotation(invalid_orientation), 0);
#endif
}

// Clone of pp::PDF::PrivateAccessibilityTextRunInfo.
struct ExpectedAccessibilityTextRunInfo {
  uint32_t len;
  gfx::RectF bounds;
  PP_PrivateDirection direction;
  pp::PDF::PrivateAccessibilityTextStyleInfo style;
};

void CompareTextRuns(
    const ExpectedAccessibilityTextRunInfo& expected_text_run,
    const pp::PDF::PrivateAccessibilityTextRunInfo& actual_text_run) {
  EXPECT_EQ(expected_text_run.len, actual_text_run.len);

  // Use EXPECT_FLOAT_EQ() here instead of direct gfx::RectF comparisons to
  // avoid having to deal with float rounding errors.
  gfx::RectF actual_bounds = RectFFromPPFloatRect(actual_text_run.bounds);
  EXPECT_FLOAT_EQ(expected_text_run.bounds.x(), actual_bounds.x());
  EXPECT_FLOAT_EQ(expected_text_run.bounds.y(), actual_bounds.y());
  EXPECT_FLOAT_EQ(expected_text_run.bounds.width(), actual_bounds.width());
  EXPECT_FLOAT_EQ(expected_text_run.bounds.height(), actual_bounds.height());

  EXPECT_EQ(expected_text_run.direction, actual_text_run.direction);

  const pp::PDF::PrivateAccessibilityTextStyleInfo& expected_style =
      expected_text_run.style;
  const pp::PDF::PrivateAccessibilityTextStyleInfo& actual_style =
      actual_text_run.style;

  EXPECT_EQ(expected_style.font_name, actual_style.font_name);
  EXPECT_EQ(expected_style.font_weight, actual_style.font_weight);
  EXPECT_EQ(expected_style.render_mode, actual_style.render_mode);
  EXPECT_FLOAT_EQ(expected_style.font_size, actual_style.font_size);
  EXPECT_EQ(expected_style.fill_color, actual_style.fill_color);
  EXPECT_EQ(expected_style.stroke_color, actual_style.stroke_color);
  EXPECT_EQ(expected_style.is_italic, actual_style.is_italic);
  EXPECT_EQ(expected_style.is_bold, actual_style.is_bold);
}

template <typename T>
void PopulateTextObjects(const std::vector<gfx::Range>& ranges,
                         std::vector<T>* text_objects) {
  text_objects->resize(ranges.size());
  for (size_t i = 0; i < ranges.size(); ++i) {
    (*text_objects)[i].start_char_index = ranges[i].start();
    (*text_objects)[i].char_count = ranges[i].end() - ranges[i].start();
  }
}

base::FilePath GetThumbnailTestData(const std::string& expectation_file_prefix,
                                    size_t page_index,
                                    float device_pixel_ratio) {
  std::string file_dir = base::StringPrintf("%.1fx", device_pixel_ratio);
  std::string file_name = base::StringPrintf(
      "%s_expected.pdf.%zu.png", expectation_file_prefix.c_str(), page_index);
  base::FilePath root_path;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path))
    return base::FilePath();
  return root_path.Append(FILE_PATH_LITERAL("pdf"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("thumbnail"))
      .AppendASCII(file_dir)
      .AppendASCII(file_name);
}

}  // namespace

using PDFiumPageTest = PDFiumTestBase;

TEST_F(PDFiumPageTest, Constructor) {
  PDFiumPage page(/*engine=*/nullptr, 2);
  EXPECT_EQ(page.index(), 2);
  EXPECT_TRUE(page.rect().IsEmpty());
  EXPECT_FALSE(page.available());
}

class PDFiumPageLinkTest : public PDFiumTestBase {
 public:
  PDFiumPageLinkTest() = default;
  PDFiumPageLinkTest(const PDFiumPageLinkTest&) = delete;
  PDFiumPageLinkTest& operator=(const PDFiumPageLinkTest&) = delete;
  ~PDFiumPageLinkTest() override = default;

  const std::vector<PDFiumPage::Link>& GetLinks(PDFiumEngine& engine,
                                                int page_index) {
    PDFiumPage& page = GetPDFiumPageForTest(engine, page_index);
    page.CalculateLinks();
    return page.links_;
  }
};

TEST_F(PDFiumPageLinkTest, TestLinkGeneration) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  bool is_chromeos = IsRunningOnChromeOS();

  const std::vector<PDFiumPage::Link>& links = GetLinks(*engine, 0);
  ASSERT_EQ(3u, links.size());

  const PDFiumPage::Link& link = links[0];
  EXPECT_EQ("http://yahoo.com", link.target.url);
  EXPECT_EQ(7, link.start_char_index);
  EXPECT_EQ(16, link.char_count);
  ASSERT_EQ(1u, link.bounding_rects.size());
  if (is_chromeos) {
    EXPECT_EQ(gfx::Rect(75, 192, 110, 15), link.bounding_rects[0]);
  } else {
    EXPECT_EQ(gfx::Rect(75, 191, 110, 16), link.bounding_rects[0]);
  }

  const PDFiumPage::Link& second_link = links[1];
  EXPECT_EQ("http://bing.com", second_link.target.url);
  EXPECT_EQ(52, second_link.start_char_index);
  EXPECT_EQ(15, second_link.char_count);
  ASSERT_EQ(1u, second_link.bounding_rects.size());
  if (is_chromeos) {
    EXPECT_EQ(gfx::Rect(131, 120, 138, 22), second_link.bounding_rects[0]);
  } else {
    EXPECT_EQ(gfx::Rect(131, 121, 138, 20), second_link.bounding_rects[0]);
  }

  const PDFiumPage::Link& third_link = links[2];
  EXPECT_EQ("http://google.com", third_link.target.url);
  EXPECT_EQ(92, third_link.start_char_index);
  EXPECT_EQ(17, third_link.char_count);
  ASSERT_EQ(1u, third_link.bounding_rects.size());
  EXPECT_EQ(gfx::Rect(82, 67, 161, 21), third_link.bounding_rects[0]);
}

TEST_F(PDFiumPageLinkTest, TestAnnotLinkGeneration) {
  struct ExpectedLink {
    int32_t start_char_index;
    int32_t char_count;
    std::vector<gfx::Rect> bounding_rects;
    std::string url;
    int page;
    float y_in_pixels;
  };
  static ExpectedLink expected_links[] = {
      {144, 38, {{99, 436, 236, 13}}, "https://pdfium.googlesource.com/pdfium"},
      {27, 38, {{112, 215, 617, 28}}, "", 1, 89.333336},
      {65, 27, {{93, 334, 174, 21}}, "https://www.adobe.com"},
      {253,
       18,
       {{242, 455, 1, 18}, {242, 472, 1, 15}},
       "https://cs.chromium.org"},
      {-1, 0, {{58, 926, 28, 27}}, "https://www.google.com"}};
  if (IsRunningOnChromeOS()) {
    expected_links[0].bounding_rects[0] = {99, 436, 236, 14};
  }
  static constexpr size_t kExpectedLinkCount = base::size(expected_links);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());

  const std::vector<PDFiumPage::Link>& links = GetLinks(*engine, 0);
  ASSERT_EQ(kExpectedLinkCount, links.size());

  for (size_t i = 0; i < kExpectedLinkCount; ++i) {
    const PDFiumPage::Link& actual_current_link = links[i];
    const ExpectedLink& expected_current_link = expected_links[i];
    EXPECT_EQ(expected_current_link.start_char_index,
              actual_current_link.start_char_index);
    EXPECT_EQ(expected_current_link.char_count, actual_current_link.char_count);
    size_t bounds_size = actual_current_link.bounding_rects.size();
    ASSERT_EQ(expected_current_link.bounding_rects.size(), bounds_size);
    for (size_t bounds_index = 0; bounds_index < bounds_size; ++bounds_index) {
      EXPECT_EQ(expected_current_link.bounding_rects[bounds_index],
                actual_current_link.bounding_rects[bounds_index]);
    }
    EXPECT_EQ(expected_current_link.url, actual_current_link.target.url);
    if (actual_current_link.target.url.empty()) {
      EXPECT_EQ(expected_current_link.page, actual_current_link.target.page);
      ASSERT_TRUE(actual_current_link.target.y_in_pixels.has_value());
      EXPECT_FLOAT_EQ(expected_current_link.y_in_pixels,
                      actual_current_link.target.y_in_pixels.value());
    }
  }
}

using PDFiumPageImageTest = PDFiumTestBase;

TEST_F(PDFiumPageImageTest, TestCalculateImages) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("image_alt_text.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(3u, page.images_.size());
  EXPECT_EQ(gfx::Rect(380, 78, 67, 68), page.images_[0].bounding_rect);
  EXPECT_EQ("Image 1", page.images_[0].alt_text);
  EXPECT_EQ(gfx::Rect(380, 385, 27, 28), page.images_[1].bounding_rect);
  EXPECT_EQ("Image 2", page.images_[1].alt_text);
  EXPECT_EQ(gfx::Rect(380, 678, 1, 1), page.images_[2].bounding_rect);
  EXPECT_EQ("Image 3", page.images_[2].alt_text);
}

TEST_F(PDFiumPageImageTest, TestImageAltText) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("text_with_image.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(3u, page.images_.size());
  EXPECT_EQ(gfx::Rect(380, 78, 67, 68), page.images_[0].bounding_rect);
  EXPECT_EQ("Image 1", page.images_[0].alt_text);
  EXPECT_EQ(gfx::Rect(380, 385, 27, 28), page.images_[1].bounding_rect);
  EXPECT_EQ("", page.images_[1].alt_text);
  EXPECT_EQ(gfx::Rect(380, 678, 1, 1), page.images_[2].bounding_rect);
  EXPECT_EQ("", page.images_[2].alt_text);
}

using PDFiumPageTextTest = PDFiumTestBase;

TEST_F(PDFiumPageTextTest, TestTextRunBounds) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("leading_trailing_spaces_per_text_run.pdf"));
  ASSERT_TRUE(engine);

  constexpr int kFirstRunStartIndex = 0;
  constexpr int kFirstRunEndIndex = 20;
  constexpr int kPageIndex = 0;
  base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo> text_run_info_1 =
      engine->GetTextRunInfo(kPageIndex, kFirstRunStartIndex);
  ASSERT_TRUE(text_run_info_1.has_value());

  const auto& actual_text_run_1 = text_run_info_1.value();
  EXPECT_EQ(21u, actual_text_run_1.len);

  EXPECT_TRUE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kFirstRunStartIndex)));
  gfx::RectF text_run_bounds = RectFFromPPFloatRect(actual_text_run_1.bounds);
  EXPECT_TRUE(text_run_bounds.Contains(
      engine->GetCharBounds(kPageIndex, kFirstRunStartIndex)));

  // Last non-space character should fall in the bounding box of the text run.
  // Text run looks like this:
  // " Hello, world! \r\n "<17 characters><first Tj>
  // " \r\n "<4 characters><second Tj>
  // " "<1 character><third Tj starting spaces>
  // Finally generated text run: " Hello, world! \r\n \r\n "
  constexpr int kFirstRunLastNonSpaceCharIndex = 13;
  EXPECT_FALSE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kFirstRunLastNonSpaceCharIndex)));
  EXPECT_TRUE(text_run_bounds.Contains(
      engine->GetCharBounds(kPageIndex, kFirstRunLastNonSpaceCharIndex)));

  EXPECT_TRUE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kFirstRunEndIndex)));
  gfx::RectF end_char_rect =
      engine->GetCharBounds(kPageIndex, kFirstRunEndIndex);
  EXPECT_FALSE(text_run_bounds.Contains(end_char_rect));
  // Equals to the length of the previous text run.
  constexpr int kSecondRunStartIndex = 21;
  constexpr int kSecondRunEndIndex = 36;
  // Test the properties of second text run.
  // Note: The leading spaces in second text run are accounted for in the end
  // of first text run. Hence we won't see a space leading the second text run.
  base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo> text_run_info_2 =
      engine->GetTextRunInfo(kPageIndex, kSecondRunStartIndex);
  ASSERT_TRUE(text_run_info_2.has_value());

  const auto& actual_text_run_2 = text_run_info_2.value();
  EXPECT_EQ(16u, actual_text_run_2.len);

  EXPECT_FALSE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kSecondRunStartIndex)));
  text_run_bounds = RectFFromPPFloatRect(actual_text_run_2.bounds);
  EXPECT_TRUE(text_run_bounds.Contains(
      engine->GetCharBounds(kPageIndex, kSecondRunStartIndex)));

  // Last non-space character should fall in the bounding box of the text run.
  // Text run looks like this:
  // "Goodbye, world! "<19 characters><first Tj>
  // Finally generated text run: "Goodbye, world! "
  constexpr int kSecondRunLastNonSpaceCharIndex = 35;
  EXPECT_FALSE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kSecondRunLastNonSpaceCharIndex)));
  EXPECT_TRUE(text_run_bounds.Contains(
      engine->GetCharBounds(kPageIndex, kSecondRunLastNonSpaceCharIndex)));

  EXPECT_TRUE(base::IsUnicodeWhitespace(
      engine->GetCharUnicode(kPageIndex, kSecondRunEndIndex)));
  EXPECT_FALSE(text_run_bounds.Contains(
      engine->GetCharBounds(kPageIndex, kSecondRunEndIndex)));
}

TEST_F(PDFiumPageTextTest, GetTextRunInfo) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);

  int current_char_index = 0;

  pp::PDF::PrivateAccessibilityTextStyleInfo expected_style_1 = {
      "Times-Roman",
      0,
      PP_TEXTRENDERINGMODE_FILL,
      12,
      0xff000000,
      0xff000000,
      false,
      false};
  pp::PDF::PrivateAccessibilityTextStyleInfo expected_style_2 = {
      "Helvetica", 0,    PP_TEXTRENDERINGMODE_FILL, 16, 0xff000000, 0xff000000,
      false,       false};
  // The links span from [7, 22], [52, 66] and [92, 108] with 16, 15 and 17
  // text run lengths respectively. There are text runs preceding and
  // succeeding them.
  ExpectedAccessibilityTextRunInfo expected_text_runs[] = {
      {7, gfx::RectF(26.666666f, 189.333333f, 38.666672f, 13.333344f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {16, gfx::RectF(70.666664f, 189.333333f, 108.0f, 14.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {20, gfx::RectF(181.333333f, 189.333333f, 117.333333f, 14.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {9, gfx::RectF(28.0f, 117.33334f, 89.333328f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {15, gfx::RectF(126.66666f, 117.33334f, 137.33334f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {20, gfx::RectF(266.66666f, 118.66666f, 169.33334f, 18.666664f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {5, gfx::RectF(28.0f, 65.333336f, 40.0f, 18.666664f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {17, gfx::RectF(77.333336f, 64.0f, 160.0f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2}};

  if (IsRunningOnChromeOS()) {
    expected_text_runs[4].bounds =
        gfx::RectF(126.66666f, 117.33334f, 137.33334f, 21.33334f);
    expected_text_runs[5].bounds =
        gfx::RectF(266.66666f, 118.66666f, 170.66666f, 20.0f);
    expected_text_runs[7].bounds =
        gfx::RectF(77.333336f, 64.0f, 160.0f, 21.33333f);
  }

  // Test negative char index returns nullopt
  base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo>
      text_run_info_result = engine->GetTextRunInfo(0, -1);
  ASSERT_FALSE(text_run_info_result.has_value());

  // Test valid char index returns expected text run info and expected text
  // style info
  for (const auto& expected_text_run : expected_text_runs) {
    text_run_info_result = engine->GetTextRunInfo(0, current_char_index);
    ASSERT_TRUE(text_run_info_result.has_value());
    const auto& actual_text_run = text_run_info_result.value();
    CompareTextRuns(expected_text_run, actual_text_run);
    current_char_index += actual_text_run.len;
  }

  // Test char index outside char range returns nullopt
  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  EXPECT_EQ(page.GetCharCount(), current_char_index);
  text_run_info_result = engine->GetTextRunInfo(0, current_char_index);
  ASSERT_FALSE(text_run_info_result.has_value());
}

TEST_F(PDFiumPageTextTest, TestHighlightTextRunInfo) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("highlights.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  // Highlights span across text run indices 0, 2 and 3.
  static const pp::PDF::PrivateAccessibilityTextStyleInfo kExpectedStyle = {
      "Helvetica", 0,    PP_TEXTRENDERINGMODE_FILL, 16, 0xff000000, 0xff000000,
      false,       false};
  ExpectedAccessibilityTextRunInfo expected_text_runs[] = {
      {5, gfx::RectF(1.3333334f, 198.66667f, 46.666668f, 14.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, kExpectedStyle},
      {7, gfx::RectF(50.666668f, 198.66667f, 47.999996f, 17.333328f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, kExpectedStyle},
      {7, gfx::RectF(106.66666f, 198.66667f, 73.333336f, 18.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, kExpectedStyle},
      {2, gfx::RectF(181.33333f, 202.66667f, 16.0f, 14.66667f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_NONE, kExpectedStyle},
      {2, gfx::RectF(198.66667f, 202.66667f, 21.333328f, 10.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, kExpectedStyle}};

  if (IsRunningOnChromeOS()) {
    expected_text_runs[2].bounds =
        gfx::RectF(106.66666f, 198.66667f, 73.333336f, 19.999985f);
    expected_text_runs[4].bounds =
        gfx::RectF(198.66667f, 201.33333f, 21.333328f, 12.000015f);
  }

  int current_char_index = 0;
  for (const auto& expected_text_run : expected_text_runs) {
    base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo>
        text_run_info_result = engine->GetTextRunInfo(0, current_char_index);
    ASSERT_TRUE(text_run_info_result.has_value());
    const auto& actual_text_run = text_run_info_result.value();
    CompareTextRuns(expected_text_run, actual_text_run);
    current_char_index += actual_text_run.len;
  }
}

using PDFiumPageHighlightTest = PDFiumTestBase;

TEST_F(PDFiumPageHighlightTest, TestPopulateHighlights) {
  struct ExpectedHighlight {
    int32_t start_char_index;
    int32_t char_count;
    gfx::Rect bounding_rect;
    uint32_t color;
  };

  constexpr uint32_t kHighlightDefaultColor = MakeARGB(255, 255, 255, 0);
  constexpr uint32_t kHighlightRedColor = MakeARGB(102, 230, 0, 0);
  constexpr uint32_t kHighlightNoColor = MakeARGB(0, 0, 0, 0);
  static const ExpectedHighlight kExpectedHighlights[] = {
      {0, 5, {5, 196, 49, 26}, kHighlightDefaultColor},
      {12, 7, {110, 196, 77, 26}, kHighlightRedColor},
      {20, 1, {192, 196, 13, 26}, kHighlightNoColor}};

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("highlights.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.PopulateAnnotations();
  ASSERT_EQ(base::size(kExpectedHighlights), page.highlights_.size());

  for (size_t i = 0; i < page.highlights_.size(); ++i) {
    ASSERT_EQ(kExpectedHighlights[i].start_char_index,
              page.highlights_[i].start_char_index);
    ASSERT_EQ(kExpectedHighlights[i].char_count,
              page.highlights_[i].char_count);
    EXPECT_EQ(kExpectedHighlights[i].bounding_rect,
              page.highlights_[i].bounding_rect);
    ASSERT_EQ(kExpectedHighlights[i].color, page.highlights_[i].color);
  }
}

using PDFiumPageTextFieldTest = PDFiumTestBase;

TEST_F(PDFiumPageTextFieldTest, TestPopulateTextFields) {
  struct ExpectedTextField {
    const char* name;
    const char* value;
    gfx::Rect bounding_rect;
    int flags;
  };

  static const ExpectedTextField kExpectedTextFields[] = {
      {"Text Box", "Text", {138, 230, 135, 41}, 0},
      {"ReadOnly", "Elephant", {138, 163, 135, 41}, 1},
      {"Required", "Required Field", {138, 303, 135, 34}, 2},
      {"Password", "", {138, 356, 135, 35}, 8192}};

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_text_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.PopulateAnnotations();
  size_t text_fields_count = page.text_fields_.size();
  ASSERT_EQ(base::size(kExpectedTextFields), text_fields_count);

  for (size_t i = 0; i < text_fields_count; ++i) {
    EXPECT_EQ(kExpectedTextFields[i].name, page.text_fields_[i].name);
    EXPECT_EQ(kExpectedTextFields[i].value, page.text_fields_[i].value);
    EXPECT_EQ(kExpectedTextFields[i].bounding_rect,
              page.text_fields_[i].bounding_rect);
    EXPECT_EQ(kExpectedTextFields[i].flags, page.text_fields_[i].flags);
  }
}

using PDFiumPageChoiceFieldTest = PDFiumTestBase;

TEST_F(PDFiumPageChoiceFieldTest, TestPopulateChoiceFields) {
  struct ExpectedChoiceFieldOption {
    const char* name;
    bool is_selected;
  };

  struct ExpectedChoiceField {
    const char* name;
    std::vector<struct ExpectedChoiceFieldOption> options;
    gfx::Rect bounding_rect;
    int flags;
  };

  static const ExpectedChoiceField kExpectedChoiceFields[] = {
      {"Listbox_SingleSelect",
       {{"Foo", false}, {"Bar", false}, {"Qux", false}},
       {138, 296, 135, 41},
       0},
      {"Combo1",
       {{"Apple", false}, {"Banana", true}, {"Cherry", false}},
       {138, 230, 135, 41},
       131072},
      {"Listbox_ReadOnly",
       {{"Dog", false}, {"Elephant", false}, {"Frog", false}},
       {138, 96, 135, 41},
       1},
      {"Listbox_MultiSelectMultipleIndices",
       {
           {"Albania", false},
           {"Belgium", true},
           {"Croatia", false},
           {"Denmark", true},
           {"Estonia", false},
       },
       {138, 430, 135, 41},
       2097152},
      {"Listbox_MultiSelectMultipleValues",
       {
           {"Alpha", false},
           {"Beta", false},
           {"Gamma", true},
           {"Delta", false},
           {"Epsilon", true},
       },
       {138, 496, 135, 41},
       2097152},
      {"Listbox_MultiSelectMultipleMismatch",
       {
           {"Alligator", true},
           {"Bear", false},
           {"Cougar", true},
           {"Deer", false},
           {"Echidna", false},
       },
       {138, 563, 135, 41},
       2097152}};

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_choice_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.PopulateAnnotations();
  size_t choice_fields_count = page.choice_fields_.size();
  ASSERT_EQ(base::size(kExpectedChoiceFields), choice_fields_count);

  for (size_t i = 0; i < choice_fields_count; ++i) {
    EXPECT_EQ(kExpectedChoiceFields[i].name, page.choice_fields_[i].name);
    size_t choice_field_options_count = page.choice_fields_[i].options.size();
    ASSERT_EQ(base::size(kExpectedChoiceFields[i].options),
              choice_field_options_count);
    for (size_t j = 0; j < choice_field_options_count; ++j) {
      EXPECT_EQ(kExpectedChoiceFields[i].options[j].name,
                page.choice_fields_[i].options[j].name);
      EXPECT_EQ(kExpectedChoiceFields[i].options[j].is_selected,
                page.choice_fields_[i].options[j].is_selected);
    }
    EXPECT_EQ(kExpectedChoiceFields[i].bounding_rect,
              page.choice_fields_[i].bounding_rect);
    EXPECT_EQ(kExpectedChoiceFields[i].flags, page.choice_fields_[i].flags);
  }
}

using PDFiumPageButtonTest = PDFiumTestBase;

TEST_F(PDFiumPageButtonTest, TestPopulateButtons) {
  struct ExpectedButton {
    const char* name;
    const char* value;
    int type;
    int flags;
    bool is_checked;
    uint32_t control_count;
    int control_index;
    gfx::Rect bounding_rect;
  };

  static const ExpectedButton kExpectedButtons[] = {{"readOnlyCheckbox",
                                                     "Yes",
                                                     FPDF_FORMFIELD_CHECKBOX,
                                                     1,
                                                     true,
                                                     1,
                                                     0,
                                                     {185, 43, 28, 28}},
                                                    {"checkbox",
                                                     "Yes",
                                                     FPDF_FORMFIELD_CHECKBOX,
                                                     2,
                                                     false,
                                                     1,
                                                     0,
                                                     {185, 96, 28, 28}},
                                                    {"RadioButton",
                                                     "value1",
                                                     FPDF_FORMFIELD_RADIOBUTTON,
                                                     49154,
                                                     false,
                                                     2,
                                                     0,
                                                     {185, 243, 28, 28}},
                                                    {"RadioButton",
                                                     "value2",
                                                     FPDF_FORMFIELD_RADIOBUTTON,
                                                     49154,
                                                     true,
                                                     2,
                                                     1,
                                                     {252, 243, 27, 28}},
                                                    {"PushButton",
                                                     "",
                                                     FPDF_FORMFIELD_PUSHBUTTON,
                                                     65536,
                                                     false,
                                                     0,
                                                     -1,
                                                     {118, 270, 55, 67}}};

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_buttons.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.PopulateAnnotations();
  size_t buttons_count = page.buttons_.size();
  ASSERT_EQ(base::size(kExpectedButtons), buttons_count);

  for (size_t i = 0; i < buttons_count; ++i) {
    EXPECT_EQ(kExpectedButtons[i].name, page.buttons_[i].name);
    EXPECT_EQ(kExpectedButtons[i].value, page.buttons_[i].value);
    EXPECT_EQ(kExpectedButtons[i].type, page.buttons_[i].type);
    EXPECT_EQ(kExpectedButtons[i].flags, page.buttons_[i].flags);
    EXPECT_EQ(kExpectedButtons[i].is_checked, page.buttons_[i].is_checked);
    EXPECT_EQ(kExpectedButtons[i].control_count,
              page.buttons_[i].control_count);
    EXPECT_EQ(kExpectedButtons[i].control_index,
              page.buttons_[i].control_index);
    EXPECT_EQ(kExpectedButtons[i].bounding_rect,
              page.buttons_[i].bounding_rect);
  }
}

using PDFiumPageOverlappingTest = PDFiumTestBase;

// The following scenarios are covered across both test cases:
// 1. Links overlapping amongst themselves.
// 2. Highlights overlapping amongst themselves.
// 3. Links partially and completely overlapping with highlights.
// 4. Adjacent annotations.
TEST_F(PDFiumPageOverlappingTest, CountPartialOverlaps) {
  static const std::vector<gfx::Range> kLinkRanges = {
      {0, 10}, {13, 25}, {37, 52}, {71, 84}, {93, 113}};
  static const std::vector<gfx::Range> kHighlightRanges = {
      {4, 13}, {8, 15}, {14, 22}, {37, 73}, {49, 95}, {80, 101}};
  std::vector<PDFiumPage::Link> links;
  std::vector<PDFiumPage::Highlight> highlights;
  PopulateTextObjects(kLinkRanges, &links);
  PopulateTextObjects(kHighlightRanges, &highlights);
  ASSERT_EQ(15u, PDFiumPage::CountLinkHighlightOverlaps(links, highlights));
}

TEST_F(PDFiumPageOverlappingTest, CountCompleteOverlaps) {
  static const std::vector<gfx::Range> kLinkRanges = {
      {0, 15}, {25, 40}, {30, 50}, {50, 67}, {61, 72}, {67, 81}};
  static const std::vector<gfx::Range> kHighlightRanges = {
      {6, 25}, {25, 40}, {30, 50}, {50, 83}};
  std::vector<PDFiumPage::Link> links;
  std::vector<PDFiumPage::Highlight> highlights;
  PopulateTextObjects(kLinkRanges, &links);
  PopulateTextObjects(kHighlightRanges, &highlights);
  ASSERT_EQ(12u, PDFiumPage::CountLinkHighlightOverlaps(links, highlights));
}

class PDFiumPageThumbnailTest : public PDFiumTestBase {
 public:
  PDFiumPageThumbnailTest() = default;
  PDFiumPageThumbnailTest(const PDFiumPageThumbnailTest&) = delete;
  PDFiumPageThumbnailTest& operator=(const PDFiumPageThumbnailTest&) = delete;
  ~PDFiumPageThumbnailTest() override = default;

  void TestGenerateThumbnail(PDFiumEngine& engine,
                             size_t page_index,
                             float device_pixel_ratio,
                             const gfx::Size& expected_thumbnail_size,
                             const std::string& expectation_file_prefix) {
    PDFiumPage& page = GetPDFiumPageForTest(engine, page_index);
    Thumbnail thumbnail = page.GenerateThumbnail(device_pixel_ratio);
    EXPECT_EQ(expected_thumbnail_size, gfx::Size(thumbnail.bitmap().width(),
                                                 thumbnail.bitmap().height()));
    EXPECT_EQ(device_pixel_ratio, thumbnail.device_pixel_ratio());

    base::FilePath expectation_png_file_path = GetThumbnailTestData(
        expectation_file_prefix, page_index, device_pixel_ratio);

    cc::MatchesPNGFile(thumbnail.bitmap(), expectation_png_file_path,
                       cc::ExactPixelComparator(/*discard_alpha=*/false));
  }
};

TEST_F(PDFiumPageThumbnailTest, GenerateThumbnail) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("variable_page_sizes.pdf"));
  ASSERT_EQ(7, engine->GetNumberOfPages());

  static constexpr struct {
    size_t page_index;
    float device_pixel_ratio;
    gfx::Size expected_thumbnail_size;
  } kGenerateThumbnailTestParams[] = {
      {0, 1, {108, 140}},  // ANSI Letter
      {1, 1, {108, 152}},  // ISO 216 A4
      {2, 1, {140, 140}},  // Square
      {3, 1, {540, 108}},  // Wide
      {4, 1, {108, 540}},  // Tall
      {5, 1, {1399, 46}},  // Super wide
      {6, 1, {46, 1399}},  // Super tall
      {0, 2, {216, 280}},  // ANSI Letter
      {1, 2, {214, 303}},  // ISO 216 A4
      {2, 2, {255, 255}},  // Square
      {3, 2, {571, 114}},  // Wide
      {4, 2, {114, 571}},  // Tall
      {5, 2, {1399, 46}},  // Super wide
      {6, 2, {46, 1399}},  // Super tall
  };

  for (const auto& params : kGenerateThumbnailTestParams) {
    TestGenerateThumbnail(*engine, params.page_index, params.device_pixel_ratio,
                          params.expected_thumbnail_size,
                          "variable_page_sizes");
  }
}

}  // namespace chrome_pdf
