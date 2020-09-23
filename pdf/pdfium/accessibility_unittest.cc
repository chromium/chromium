// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include <string>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/test/test_client.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

using AccessibilityTest = PDFiumTestBase;

float GetExpectedBoundsWidth(bool is_chromeos, size_t i, float expected) {
  return (is_chromeos && i == 0) ? 85.333336f : expected;
}

double GetExpectedCharWidth(bool is_chromeos, size_t i, double expected) {
  if (is_chromeos) {
    if (i == 25)
      return 13.333343;
    if (i == 26)
      return 6.666656;
  }
  return expected;
}

// NOTE: This test is sensitive to font metrics from the underlying platform.
// If changes to fonts on the system or to font code like FreeType cause this
// test to fail, please feel free to rebase the test expectations here, or
// update the GetExpected... functions above. If that becomes too much of a
// burden, consider changing the checks to just make sure the font metrics look
// sane.
TEST_F(AccessibilityTest, GetAccessibilityPage) {
  static constexpr size_t kExpectedTextRunCount = 2;
  struct {
    uint32_t len;
    double font_size;
    float bounds_x;
    float bounds_y;
    float bounds_w;
    float bounds_h;
  } static constexpr kExpectedTextRuns[] = {
      {15, 12, 26.666666f, 189.333328f, 84.000008f, 13.333344f},
      {15, 16, 28.000000f, 117.333334f, 152.000000f, 19.999992f},
  };
  static_assert(base::size(kExpectedTextRuns) == kExpectedTextRunCount,
                "Bad test expectation count");

  static constexpr size_t kExpectedCharCount = 30;
  static constexpr PP_PrivateAccessibilityCharInfo kExpectedChars[] = {
      {'H', 12}, {'e', 6.6666}, {'l', 5.3333}, {'l', 4},      {'o', 8},
      {',', 4},  {' ', 4},      {'w', 12},     {'o', 6.6666}, {'r', 6.6666},
      {'l', 4},  {'d', 9.3333}, {'!', 4},      {'\r', 0},     {'\n', 0},
      {'G', 16}, {'o', 12},     {'o', 12},     {'d', 12},     {'b', 10.6666},
      {'y', 12}, {'e', 12},     {',', 4},      {' ', 6.6666}, {'w', 16},
      {'o', 12}, {'r', 8},      {'l', 4},      {'d', 12},     {'!', 2.6666},
  };
  static_assert(base::size(kExpectedChars) == kExpectedCharCount,
                "Bad test expectation count");

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());
  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &page_objects));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(5, page_info.bounds.point.x);
  EXPECT_EQ(3, page_info.bounds.point.y);
  EXPECT_EQ(266, page_info.bounds.size.width);
  EXPECT_EQ(266, page_info.bounds.size.height);
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);

  bool is_chromeos = IsRunningOnChromeOS();

  ASSERT_EQ(kExpectedTextRunCount, text_runs.size());
  for (size_t i = 0; i < kExpectedTextRunCount; ++i) {
    const auto& expected = kExpectedTextRuns[i];
    EXPECT_EQ(expected.len, text_runs[i].len) << i;
    EXPECT_FLOAT_EQ(expected.font_size, text_runs[i].style.font_size) << i;
    EXPECT_FLOAT_EQ(expected.bounds_x, text_runs[i].bounds.point.x) << i;
    EXPECT_FLOAT_EQ(expected.bounds_y, text_runs[i].bounds.point.y) << i;
    float expected_bounds_w =
        GetExpectedBoundsWidth(is_chromeos, i, expected.bounds_w);
    EXPECT_FLOAT_EQ(expected_bounds_w, text_runs[i].bounds.size.width) << i;
    EXPECT_FLOAT_EQ(expected.bounds_h, text_runs[i].bounds.size.height) << i;
    EXPECT_EQ(PP_PRIVATEDIRECTION_LTR, text_runs[i].direction);
  }

  ASSERT_EQ(kExpectedCharCount, chars.size());
  for (size_t i = 0; i < kExpectedCharCount; ++i) {
    const auto& expected = kExpectedChars[i];
    EXPECT_EQ(expected.unicode_character, chars[i].unicode_character) << i;
    double expected_char_width =
        GetExpectedCharWidth(is_chromeos, i, expected.char_width);
    EXPECT_NEAR(expected_char_width, chars[i].char_width, 0.001) << i;
  }
}

TEST_F(AccessibilityTest, GetAccessibilityImageInfo) {
  // Clone of pp::PDF::PrivateAccessibilityImageInfo.
  static const struct {
    std::string alt_text;
    uint32_t text_run_index;
    gfx::RectF bounds;
  } kExpectedImageInfo[] = {{"Image 1", 0, {380, 78, 67, 68}},
                            {"Image 2", 0, {380, 385, 27, 28}},
                            {"Image 3", 0, {380, 678, 1, 1}}};

  static constexpr gfx::Rect kExpectedPageRect(5, 3, 816, 1056);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("image_alt_text.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &page_objects));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(kExpectedPageRect, RectFromPPRect(page_info.bounds));
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);
  ASSERT_EQ(page_objects.images.size(), base::size(kExpectedImageInfo));

  for (size_t i = 0; i < page_objects.images.size(); ++i) {
    EXPECT_EQ(page_objects.images[i].alt_text, kExpectedImageInfo[i].alt_text);
    EXPECT_EQ(kExpectedImageInfo[i].bounds,
              RectFFromPPFloatRect(page_objects.images[i].bounds));
    EXPECT_EQ(page_objects.images[i].text_run_index,
              kExpectedImageInfo[i].text_run_index);
  }
}

TEST_F(AccessibilityTest, GetUnderlyingTextRangeForRect) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);

  // The test rect spans across [0, 4] char indices.
  int start_index = -1;
  int char_count = 0;
  EXPECT_TRUE(page.GetUnderlyingTextRangeForRect(
      gfx::RectF(20.0f, 50.0f, 26.0f, 8.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(char_count, 5);

  // The input rectangle is spanning across multiple lines.
  // GetUnderlyingTextRangeForRect() should return only the char indices
  // of first line.
  start_index = -1;
  char_count = 0;
  EXPECT_TRUE(page.GetUnderlyingTextRangeForRect(
      gfx::RectF(20.0f, 0.0f, 26.0f, 58.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(char_count, 5);

  // There is no text below this rectangle. So, GetUnderlyingTextRangeForRect()
  // will return false and not change the dummy values set here.
  start_index = -9;
  char_count = -10;
  EXPECT_FALSE(page.GetUnderlyingTextRangeForRect(
      gfx::RectF(10.0f, 10.0f, 0.0f, 0.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, -9);
  EXPECT_EQ(char_count, -10);
}

// This class overrides TestClient to record points received when a scroll
// call is made by tests.
class ScrollEnabledTestClient : public TestClient {
 public:
  ScrollEnabledTestClient() = default;
  ~ScrollEnabledTestClient() override = default;

  // Records the scroll delta received in a ScrollBy action request from tests.
  void ScrollBy(const gfx::Vector2d& scroll_delta) override {
    received_scroll_delta_ = scroll_delta;
  }

  // Returns the scroll delta received in a ScrollBy action for validation in
  // tests.
  const gfx::Vector2d& GetScrollRequestDelta() const {
    return received_scroll_delta_;
  }

 private:
  gfx::Vector2d received_scroll_delta_;
};

TEST_F(AccessibilityTest, TestScrollIntoViewActionHandling) {
  // This test checks that accessibility scroll action is passed
  // on to the ScrollEnabledTestClient implementation.
  ScrollEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({400, 400});
  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE;
  action_data.target_rect = {{120, 0}, {10, 10}};

  // Horizontal and Vertical scroll alignment of none should not scroll.
  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(0, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(120, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(120, -400), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-280, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-280, -400), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-80, -200), client.GetScrollRequestDelta());

  // Simulate a 150% zoom update in the PDFiumEngine.
  engine->PluginSizeUpdated({600, 600});

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(0, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(120, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(120, -600), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-480, 0), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-480, -600), client.GetScrollRequestDelta());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-180, -300), client.GetScrollRequestDelta());
}

TEST_F(AccessibilityTest, TestScrollToNearestEdge) {
  ScrollEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({400, 400});
  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE;

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE;
  // Point which is in the middle of the viewport.
  action_data.target_rect = {{200, 200}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(200, 200), client.GetScrollRequestDelta());

  // Point which is near the top left of the viewport.
  action_data.target_rect = {{199, 199}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(199, 199), client.GetScrollRequestDelta());

  // Point which is near the top right of the viewport
  action_data.target_rect = {{201, 199}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-199, 199), client.GetScrollRequestDelta());

  // Point which is near the bottom left of the viewport.
  action_data.target_rect = {{199, 201}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(199, -199), client.GetScrollRequestDelta());

  // Point which is near the bottom right of the viewport
  action_data.target_rect = {{201, 201}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-199, -199), client.GetScrollRequestDelta());
}

TEST_F(AccessibilityTest, TestScrollToGlobalPoint) {
  ScrollEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({400, 400});
  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_GLOBAL_POINT;

  // Scroll up if global point is below the target rect
  action_data.target_rect = {{201, 201}, {10, 10}};
  action_data.target_point = {230, 230};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(-29, -29), client.GetScrollRequestDelta());

  // Scroll down if global point is above the target rect
  action_data.target_rect = {{230, 230}, {10, 10}};
  action_data.target_point = {201, 201};
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(gfx::Vector2d(29, 29), client.GetScrollRequestDelta());
}

// This class is required to just override the NavigateTo
// functionality for testing in a specific way. It will
// keep the TestClient class clean for extension by others.
class NavigationEnabledTestClient : public TestClient {
 public:
  NavigationEnabledTestClient() = default;
  ~NavigationEnabledTestClient() override = default;

  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override {
    url_ = url;
    disposition_ = disposition;
  }

  void NavigateToDestination(int page,
                             const float* x_in_pixels,
                             const float* y_in_pixels,
                             const float* zoom) override {
    page_ = page;
    if (x_in_pixels)
      x_in_pixels_ = *x_in_pixels;
    if (y_in_pixels)
      y_in_pixels_ = *y_in_pixels;
    if (zoom)
      zoom_ = *zoom;
  }

  const std::string& url() const { return url_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  int page() const { return page_; }
  int x_in_pixels() const { return x_in_pixels_; }
  int y_in_pixels() const { return y_in_pixels_; }
  float zoom() const { return zoom_; }

 private:
  std::string url_;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  int page_ = -1;
  float x_in_pixels_ = 0;
  float y_in_pixels_ = 0;
  float zoom_ = 0;
};

TEST_F(AccessibilityTest, TestWebLinkClickActionHandling) {
  NavigationEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);

  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION;
  action_data.page_index = 0;
  action_data.annotation_type = PP_PdfAccessibilityAnnotationType::PP_PDF_LINK;
  action_data.annotation_index = 0;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ("http://yahoo.com", client.url());
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB, client.disposition());
}

TEST_F(AccessibilityTest, TestInternalLinkClickActionHandling) {
  NavigationEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION;
  action_data.page_index = 0;
  action_data.annotation_type = PP_PdfAccessibilityAnnotationType::PP_PDF_LINK;
  action_data.annotation_index = 1;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(1, client.page());
  EXPECT_EQ(266, client.x_in_pixels());
  EXPECT_EQ(89, client.y_in_pixels());
  EXPECT_FLOAT_EQ(1.75, client.zoom());
  EXPECT_TRUE(client.url().empty());
}

TEST_F(AccessibilityTest, GetAccessibilityLinkInfo) {
  // Clone of pp::PDF::PrivateAccessibilityLinkInfo.
  struct {
    std::string url;
    uint32_t index_in_page;
    uint32_t text_run_index;
    uint32_t text_run_count;
    gfx::RectF bounds;
  } expected_link_info[] = {{"http://yahoo.com", 0, 1, 1, {75, 191, 110, 16}},
                            {"http://bing.com", 1, 4, 1, {131, 121, 138, 20}},
                            {"http://google.com", 2, 7, 1, {82, 67, 161, 21}}};

  if (IsRunningOnChromeOS()) {
    expected_link_info[0].bounds = {75, 192, 110, 15};
    expected_link_info[1].bounds = {131, 120, 138, 22};
  }

  static constexpr gfx::Rect kExpectedPageRect(5, 3, 533, 266);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &page_objects));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(kExpectedPageRect, RectFromPPRect(page_info.bounds));
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);
  ASSERT_EQ(page_objects.links.size(), base::size(expected_link_info));

  for (size_t i = 0; i < page_objects.links.size(); ++i) {
    const pp::PDF::PrivateAccessibilityLinkInfo& link_info =
        page_objects.links[i];
    EXPECT_EQ(link_info.url, expected_link_info[i].url);
    EXPECT_EQ(link_info.index_in_page, expected_link_info[i].index_in_page);
    EXPECT_EQ(expected_link_info[i].bounds,
              RectFFromPPFloatRect(link_info.bounds));
    EXPECT_EQ(link_info.text_run_index, expected_link_info[i].text_run_index);
    EXPECT_EQ(link_info.text_run_count, expected_link_info[i].text_run_count);
  }
}

TEST_F(AccessibilityTest, GetAccessibilityHighlightInfo) {
  constexpr uint32_t kHighlightDefaultColor = MakeARGB(255, 255, 255, 0);
  constexpr uint32_t kHighlightRedColor = MakeARGB(102, 230, 0, 0);
  constexpr uint32_t kHighlightNoColor = MakeARGB(0, 0, 0, 0);
  // Clone of pp::PDF::PrivateAccessibilityHighlightInfo.
  static const struct {
    std::string note_text;
    uint32_t index_in_page;
    uint32_t text_run_index;
    uint32_t text_run_count;
    gfx::RectF bounds;
    uint32_t color;
  } kExpectedHighlightInfo[] = {
      {"Text Note", 0, 0, 1, {5, 196, 49, 26}, kHighlightDefaultColor},
      {"", 1, 2, 1, {110, 196, 77, 26}, kHighlightRedColor},
      {"", 2, 3, 1, {192, 196, 13, 26}, kHighlightNoColor}};

  static constexpr gfx::Rect kExpectedPageRect(5, 3, 533, 266);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("highlights.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &page_objects));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(kExpectedPageRect, RectFromPPRect(page_info.bounds));
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);
  ASSERT_EQ(page_objects.highlights.size(), base::size(kExpectedHighlightInfo));

  for (size_t i = 0; i < page_objects.highlights.size(); ++i) {
    const pp::PDF::PrivateAccessibilityHighlightInfo& highlight_info =
        page_objects.highlights[i];
    EXPECT_EQ(highlight_info.index_in_page,
              kExpectedHighlightInfo[i].index_in_page);
    EXPECT_EQ(kExpectedHighlightInfo[i].bounds,
              RectFFromPPFloatRect(highlight_info.bounds));
    EXPECT_EQ(highlight_info.text_run_index,
              kExpectedHighlightInfo[i].text_run_index);
    EXPECT_EQ(highlight_info.text_run_count,
              kExpectedHighlightInfo[i].text_run_count);
    EXPECT_EQ(highlight_info.color, kExpectedHighlightInfo[i].color);
    EXPECT_EQ(highlight_info.note_text, kExpectedHighlightInfo[i].note_text);
  }
}

TEST_F(AccessibilityTest, GetAccessibilityTextFieldInfo) {
  // Clone of pp::PDF::PrivateAccessibilityTextFieldInfo.
  static const struct {
    std::string name;
    std::string value;
    bool is_read_only;
    bool is_required;
    bool is_password;
    uint32_t index_in_page;
    uint32_t text_run_index;
    gfx::RectF bounds;
  } kExpectedTextFieldInfo[] = {
      {"Text Box", "Text", false, false, false, 0, 5, {138, 230, 135, 41}},
      {"ReadOnly", "Elephant", true, false, false, 1, 5, {138, 163, 135, 41}},
      {"Required",
       "Required Field",
       false,
       true,
       false,
       2,
       5,
       {138, 303, 135, 34}},
      {"Password", "", false, false, true, 3, 5, {138, 356, 135, 35}}};

  static constexpr gfx::Rect kExpectedPageRect(5, 3, 400, 400);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_text_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &page_objects));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(kExpectedPageRect, RectFromPPRect(page_info.bounds));
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);
  ASSERT_EQ(page_objects.form_fields.text_fields.size(),
            base::size(kExpectedTextFieldInfo));

  for (size_t i = 0; i < page_objects.form_fields.text_fields.size(); ++i) {
    const pp::PDF::PrivateAccessibilityTextFieldInfo& text_field_info =
        page_objects.form_fields.text_fields[i];
    EXPECT_EQ(kExpectedTextFieldInfo[i].name, text_field_info.name);
    EXPECT_EQ(kExpectedTextFieldInfo[i].value, text_field_info.value);
    EXPECT_EQ(kExpectedTextFieldInfo[i].is_read_only,
              text_field_info.is_read_only);
    EXPECT_EQ(kExpectedTextFieldInfo[i].is_required,
              text_field_info.is_required);
    EXPECT_EQ(kExpectedTextFieldInfo[i].is_password,
              text_field_info.is_password);
    EXPECT_EQ(kExpectedTextFieldInfo[i].index_in_page,
              text_field_info.index_in_page);
    EXPECT_EQ(kExpectedTextFieldInfo[i].text_run_index,
              text_field_info.text_run_index);
    EXPECT_EQ(kExpectedTextFieldInfo[i].bounds,
              RectFFromPPFloatRect(text_field_info.bounds));
  }
}

TEST_F(AccessibilityTest, TestSelectionActionHandling) {
  struct Selection {
    uint32_t start_page_index;
    uint32_t start_char_index;
    uint32_t end_page_index;
    uint32_t end_char_index;
  };

  struct TestCase {
    Selection action;
    Selection expected_result;
  };

  static constexpr TestCase kTestCases[] = {
      {{0, 0, 0, 0}, {0, 0, 0, 0}},
      {{0, 0, 1, 5}, {0, 0, 1, 5}},
      // Selection action data with invalid char index.
      // GetSelection() should return the previous selection in this case.
      {{0, 0, 0, 50}, {0, 0, 1, 5}},
      // Selection action data for reverse selection where start selection
      // index is greater than end selection index. GetSelection() should
      // return the sanitized selection value where start selection index
      // is less than end selection index.
      {{1, 10, 0, 5}, {0, 5, 1, 10}},
      {{0, 10, 0, 4}, {0, 4, 0, 10}},
      // Selection action data with invalid page index.
      // GetSelection() should return the previous selection in this case.
      {{0, 10, 2, 4}, {0, 4, 0, 10}},
  };

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  for (const auto& test_case : kTestCases) {
    PP_PdfAccessibilityActionData action_data;
    action_data.action = PP_PdfAccessibilityAction::PP_PDF_SET_SELECTION;
    const Selection& sel_action = test_case.action;
    action_data.selection_start_index.page_index = sel_action.start_page_index;
    action_data.selection_start_index.char_index = sel_action.start_char_index;
    action_data.selection_end_index.page_index = sel_action.end_page_index;
    action_data.selection_end_index.char_index = sel_action.end_char_index;

    engine->HandleAccessibilityAction(action_data);
    Selection actual_selection;
    engine->GetSelection(
        &actual_selection.start_page_index, &actual_selection.start_char_index,
        &actual_selection.end_page_index, &actual_selection.end_char_index);
    const Selection& expected_selection = test_case.expected_result;
    EXPECT_EQ(actual_selection.start_page_index,
              expected_selection.start_page_index);
    EXPECT_EQ(actual_selection.start_char_index,
              expected_selection.start_char_index);
    EXPECT_EQ(actual_selection.end_page_index,
              expected_selection.end_page_index);
    EXPECT_EQ(actual_selection.end_char_index,
              expected_selection.end_char_index);
  }
}

}  // namespace chrome_pdf
