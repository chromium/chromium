// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_utils.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  ~PDFiumPageLinkTest() override = default;

  const std::vector<PDFiumPage::Link>& GetLinks(PDFiumEngine* engine,
                                                int page_index) {
    PDFiumPage* page = GetPDFiumPageForTest(engine, page_index);
    DCHECK(page);
    page->CalculateLinks();
    return page->links_;
  }

  DISALLOW_COPY_AND_ASSIGN(PDFiumPageLinkTest);
};

TEST_F(PDFiumPageLinkTest, TestLinkGeneration) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  bool is_chromeos = IsRunningOnChromeOS();

  const std::vector<PDFiumPage::Link>& links = GetLinks(engine.get(), 0);
  ASSERT_EQ(3u, links.size());

  const PDFiumPage::Link& link = links[0];
  EXPECT_EQ("http://yahoo.com", link.target.url);
  EXPECT_EQ(7, link.start_char_index);
  EXPECT_EQ(16, link.char_count);
  ASSERT_EQ(1u, link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({75, 192, 110, 15}, link.bounding_rects[0]);
  } else {
    CompareRect({75, 191, 110, 16}, link.bounding_rects[0]);
  }

  const PDFiumPage::Link& second_link = links[1];
  EXPECT_EQ("http://bing.com", second_link.target.url);
  EXPECT_EQ(52, second_link.start_char_index);
  EXPECT_EQ(15, second_link.char_count);
  ASSERT_EQ(1u, second_link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({131, 120, 138, 22}, second_link.bounding_rects[0]);
  } else {
    CompareRect({131, 121, 138, 20}, second_link.bounding_rects[0]);
  }

  const PDFiumPage::Link& third_link = links[2];
  EXPECT_EQ("http://google.com", third_link.target.url);
  EXPECT_EQ(92, third_link.start_char_index);
  EXPECT_EQ(17, third_link.char_count);
  ASSERT_EQ(1u, third_link.bounding_rects.size());
  CompareRect({82, 67, 161, 21}, third_link.bounding_rects[0]);
}

TEST_F(PDFiumPageLinkTest, TestAnnotLinkGeneration) {
  struct ExpectedLink {
    int32_t start_char_index;
    int32_t char_count;
    std::vector<pp::Rect> bounding_rects;
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

  const std::vector<PDFiumPage::Link>& links = GetLinks(engine.get(), 0);
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
      CompareRect(expected_current_link.bounding_rects[bounds_index],
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

  PDFiumPage* page = GetPDFiumPageForTest(engine.get(), 0);
  ASSERT_TRUE(page);
  page->CalculateImages();
  ASSERT_EQ(3u, page->images_.size());
  CompareRect({380, 78, 67, 68}, page->images_[0].bounding_rect);
  EXPECT_EQ("Image 1", page->images_[0].alt_text);
  CompareRect({380, 385, 27, 28}, page->images_[1].bounding_rect);
  EXPECT_EQ("Image 2", page->images_[1].alt_text);
  CompareRect({380, 678, 1, 1}, page->images_[2].bounding_rect);
  EXPECT_EQ("Image 3", page->images_[2].alt_text);
}

TEST_F(PDFiumPageImageTest, TestImageAltText) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("text_with_image.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage* page = GetPDFiumPageForTest(engine.get(), 0);
  ASSERT_TRUE(page);
  page->CalculateImages();
  ASSERT_EQ(3u, page->images_.size());
  CompareRect({380, 78, 67, 68}, page->images_[0].bounding_rect);
  EXPECT_EQ("Image 1", page->images_[0].alt_text);
  CompareRect({380, 385, 27, 28}, page->images_[1].bounding_rect);
  EXPECT_EQ("", page->images_[1].alt_text);
  CompareRect({380, 678, 1, 1}, page->images_[2].bounding_rect);
  EXPECT_EQ("", page->images_[2].alt_text);
}

using PDFiumPageTextTest = PDFiumTestBase;

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
  pp::PDF::PrivateAccessibilityTextRunInfo expected_text_runs[] = {
      {7,
       PP_MakeFloatRectFromXYWH(26.666666f, 189.333333f, 38.666672f,
                                13.333344f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {16,
       PP_MakeFloatRectFromXYWH(70.666664f, 189.333333f, 108.0f, 14.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {20,
       PP_MakeFloatRectFromXYWH(181.333333f, 189.333333f, 117.333333f,
                                14.666672f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_1},
      {9, PP_MakeFloatRectFromXYWH(28.0f, 117.33334f, 89.333328f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {15, PP_MakeFloatRectFromXYWH(126.66666f, 117.33334f, 137.33334f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {20,
       PP_MakeFloatRectFromXYWH(266.66666f, 118.66666f, 169.33334f, 18.666664f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {5, PP_MakeFloatRectFromXYWH(28.0f, 65.333336f, 40.0f, 18.666664f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2},
      {17, PP_MakeFloatRectFromXYWH(77.333336f, 64.0f, 160.0f, 20.0f),
       PP_PrivateDirection::PP_PRIVATEDIRECTION_LTR, expected_style_2}};

  if (IsRunningOnChromeOS()) {
    expected_text_runs[4].bounds =
        PP_MakeFloatRectFromXYWH(126.66666f, 117.33334f, 137.33334f, 21.33334f);
    expected_text_runs[5].bounds =
        PP_MakeFloatRectFromXYWH(266.66666f, 118.66666f, 170.66666f, 20.0f);
    expected_text_runs[7].bounds =
        PP_MakeFloatRectFromXYWH(77.333336f, 64.0f, 160.0f, 21.33333f);
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
    const auto& text_run_info = text_run_info_result.value();
    EXPECT_EQ(expected_text_run.len, text_run_info.len);
    CompareRect(expected_text_run.bounds, text_run_info.bounds);
    const pp::PDF::PrivateAccessibilityTextStyleInfo& expected_style =
        expected_text_run.style;
    const pp::PDF::PrivateAccessibilityTextStyleInfo& text_run_info_style =
        text_run_info.style;
    EXPECT_EQ(expected_text_run.direction, text_run_info.direction);
    EXPECT_EQ(expected_style.font_name, text_run_info_style.font_name);
    EXPECT_EQ(expected_style.font_weight, text_run_info_style.font_weight);
    EXPECT_EQ(expected_style.render_mode, text_run_info_style.render_mode);
    EXPECT_EQ(expected_style.font_size, text_run_info_style.font_size);
    EXPECT_EQ(expected_style.fill_color, text_run_info_style.fill_color);
    EXPECT_EQ(expected_style.stroke_color, text_run_info_style.stroke_color);
    EXPECT_EQ(expected_style.is_italic, text_run_info_style.is_italic);
    EXPECT_EQ(expected_text_run.style.is_bold, text_run_info_style.is_bold);
    current_char_index += text_run_info.len;
  }

  // Test char index outside char range returns nullopt
  PDFiumPage* page = GetPDFiumPageForTest(engine.get(), 0);
  ASSERT_TRUE(page);
  EXPECT_EQ(page->GetCharCount(), current_char_index);
  text_run_info_result = engine->GetTextRunInfo(0, current_char_index);
  ASSERT_FALSE(text_run_info_result.has_value());
}

}  // namespace chrome_pdf
