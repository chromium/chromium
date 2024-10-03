// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(UNSAFE_BUFFERS_BUILD)
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "pdf/pdfium/pdfium_page.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "build/build_config.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "pdf/ui/thumbnail.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

namespace {

TEST(PDFiumPageHelperTest, ToPDFiumRotation) {
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kOriginal), 0);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise90), 1);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise180), 2);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise270), 3);
}

TEST(PDFiumPageHelperTest, ScopedUnloadPreventer) {
  // Should not DCHECK in its dtor due to ScopedUnloadPreventer usage.
  PDFiumPage page1(/*engine=*/nullptr, 1);
  PDFiumPage page2(/*engine=*/nullptr, 2);
  PDFiumPage::ScopedUnloadPreventer prevent_unload1(&page1);
  PDFiumPage::ScopedUnloadPreventer prevent_unload2(&page2);
  PDFiumPage::ScopedUnloadPreventer prevent_unload3(prevent_unload2);
  PDFiumPage::ScopedUnloadPreventer prevent_unload4(&page2);
  prevent_unload2 = prevent_unload1;
  prevent_unload1 = prevent_unload2;
  prevent_unload1 = prevent_unload4;
  prevent_unload4 = prevent_unload1;
  prevent_unload3 = prevent_unload4;
}

void CompareTextRuns(const AccessibilityTextRunInfo& expected_text_run,
                     const AccessibilityTextRunInfo& actual_text_run) {
  EXPECT_EQ(expected_text_run.len, actual_text_run.len);
  EXPECT_RECTF_EQ(expected_text_run.bounds, actual_text_run.bounds);
  EXPECT_EQ(expected_text_run.direction, actual_text_run.direction);

  const AccessibilityTextStyleInfo& expected_style = expected_text_run.style;
  const AccessibilityTextStyleInfo& actual_style = actual_text_run.style;

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

// Returns the page size for a `PDFiumPage`. The caller must make sure that
// `pdfium_page` is available.
gfx::SizeF GetPageSizeHelper(PDFiumPage& pdfium_page) {
  FPDF_PAGE page = pdfium_page.GetPage();
  return gfx::SizeF(FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page));
}

base::FilePath GetThumbnailTestData(const std::string& expectation_file_prefix,
                                    size_t page_index,
                                    float device_pixel_ratio,
                                    bool use_skia) {
  std::string file_dir = base::StringPrintf("%.1fx", device_pixel_ratio);
  std::string file_name = base::StringPrintf(
      "%s_expected%s.pdf.%zu.png", expectation_file_prefix.c_str(),
      use_skia ? "_skia" : "", page_index);
  return base::FilePath(FILE_PATH_LITERAL("thumbnail"))
      .AppendASCII(file_dir)
      .AppendASCII(file_name);
}

constexpr struct {
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

}  // namespace

using PDFiumPageTest = PDFiumTestBase;

TEST_P(PDFiumPageTest, Constructor) {
  PDFiumPage page(/*engine=*/nullptr, 2);
  EXPECT_EQ(page.index(), 2);
  EXPECT_TRUE(page.rect().IsEmpty());
  EXPECT_FALSE(page.available());
}

TEST_P(PDFiumPageTest, IsCharInPageBounds) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage page(engine.get(), 0);
  EXPECT_FALSE(page.available());
  EXPECT_EQ(page.GetCharCount(), 0);

  page.MarkAvailable();
  EXPECT_TRUE(page.available());
  EXPECT_EQ(page.GetCharCount(), 30);

  const gfx::RectF page_bounds = page.GetCroppedRect();
  EXPECT_EQ(page_bounds, gfx::RectF(193.33333f, 129.33333f));

  EXPECT_EQ(page.GetCharUnicode(0), static_cast<uint32_t>('H'));
  EXPECT_FALSE(page.IsCharInPageBounds(0, page_bounds));
  EXPECT_EQ(page.GetCharUnicode(12), static_cast<uint32_t>('!'));
  EXPECT_TRUE(page.IsCharInPageBounds(12, page_bounds));
  EXPECT_EQ(page.GetCharUnicode(13), static_cast<uint32_t>('\r'));
  EXPECT_TRUE(page.IsCharInPageBounds(13, page_bounds));
  EXPECT_EQ(page.GetCharUnicode(14), static_cast<uint32_t>('\n'));
  EXPECT_TRUE(page.IsCharInPageBounds(14, page_bounds));
  EXPECT_EQ(page.GetCharUnicode(15), static_cast<uint32_t>('G'));
  EXPECT_FALSE(page.IsCharInPageBounds(15, page_bounds));
  EXPECT_EQ(page.GetCharUnicode(29), static_cast<uint32_t>('!'));
  EXPECT_FALSE(page.IsCharInPageBounds(29, page_bounds));
}

TEST_P(PDFiumPageTest, GetBoundingBoxRotatedMultipage) {
  // Check getting bounding box for multiple rotated pages.
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rotated_multi_page.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(4, engine->GetNumberOfPages());

  // Rotation 0 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(0.0f, bounding_box.x());
    EXPECT_FLOAT_EQ(266.66669f, bounding_box.y());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.width());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.height());
  }

  // Rotation 90 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 1);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(266.66669f, bounding_box.x());
    EXPECT_FLOAT_EQ(666.66669f, bounding_box.y());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.width());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.height());
  }
  // Rotation 180 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 2);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(666.66669f, bounding_box.x());
    EXPECT_FLOAT_EQ(933.33337f, bounding_box.y());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.width());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.height());
  }
  // Rotation 270 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 3);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(933.33337f, bounding_box.x());
    EXPECT_FLOAT_EQ(0.0f, bounding_box.y());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.width());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.height());
  }
}

TEST_P(PDFiumPageTest, GetBoundingBoxAnnotations) {
  // Check getting the bounding box for annotations.
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  const gfx::RectF bounding_box = page.GetBoundingBox();
  EXPECT_FLOAT_EQ(92.0f, bounding_box.x());
  EXPECT_FLOAT_EQ(450.66669, bounding_box.y());
  EXPECT_FLOAT_EQ(201.33334f, bounding_box.width());
  EXPECT_FLOAT_EQ(469.33334f, bounding_box.height());
}

TEST_P(PDFiumPageTest, GetBoundingBoxBlankPage) {
  // Check getting the bounding box for a blank page. The bounding box should be
  // the crop box scaled to page pixels.
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  // The crop box is 200x200 in points.
  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  const gfx::RectF bounding_box = page.GetBoundingBox();
  EXPECT_FLOAT_EQ(0.0f, bounding_box.x());
  EXPECT_FLOAT_EQ(0.0f, bounding_box.y());
  EXPECT_FLOAT_EQ(266.66669f, bounding_box.width());
  EXPECT_FLOAT_EQ(266.66669f, bounding_box.height());
}

TEST_P(PDFiumPageTest, GetBoundingBoxCropped) {
  // Check getting the bounding box for a page with a crop box different than
  // the media box.
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("landscape_rectangles.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  const gfx::RectF bounding_box = page.GetBoundingBox();
  EXPECT_FLOAT_EQ(0.0f, bounding_box.x());
  EXPECT_FLOAT_EQ(0.0f, bounding_box.y());
  EXPECT_FLOAT_EQ(800.0f, bounding_box.width());
  EXPECT_FLOAT_EQ(533.33337f, bounding_box.height());
}

TEST_P(PDFiumPageTest, GetBoundingBoxRotatedMultipageCropped) {
  // Check getting the bounding box for a multiple rotated pages with a crop
  // box.
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rotated_multi_page_cropped.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(4, engine->GetNumberOfPages());

  // Rotation 0 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(0.0f, bounding_box.x());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.y());
    EXPECT_FLOAT_EQ(66.666672f, bounding_box.width());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.height());
  }

  // Rotation 90 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 1);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.x());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.y());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.width());
    EXPECT_FLOAT_EQ(66.666672f, bounding_box.height());
  }
  // Rotation 180 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 2);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(400.0f, bounding_box.x());
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.y());
    EXPECT_FLOAT_EQ(66.666672f, bounding_box.width());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.height());
  }
  // Rotation 270 degrees clockwise.
  {
    PDFiumPage& page = GetPDFiumPageForTest(*engine, 3);
    const gfx::RectF bounding_box = page.GetBoundingBox();
    EXPECT_FLOAT_EQ(133.33334f, bounding_box.x());
    EXPECT_FLOAT_EQ(0.0f, bounding_box.y());
    EXPECT_FLOAT_EQ(400.0f, bounding_box.width());
    EXPECT_FLOAT_EQ(66.666672f, bounding_box.height());
  }
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageTest, testing::Bool());

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

TEST_P(PDFiumPageLinkTest, LinkGeneration) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  bool using_test_fonts = UsingTestFonts();

  const std::vector<PDFiumPage::Link>& links = GetLinks(*engine, 0);
  ASSERT_EQ(3u, links.size());

  const PDFiumPage::Link& link = links[0];
  EXPECT_EQ("http://yahoo.com", link.target.url);
  EXPECT_EQ(7, link.start_char_index);
  EXPECT_EQ(16, link.char_count);
  ASSERT_EQ(1u, link.bounding_rects.size());
  if (using_test_fonts) {
    EXPECT_EQ(gfx::Rect(75, 192, 110, 15), link.bounding_rects[0]);
  } else {
    EXPECT_EQ(gfx::Rect(75, 191, 110, 16), link.bounding_rects[0]);
  }

  const PDFiumPage::Link& second_link = links[1];
  EXPECT_EQ("http://bing.com", second_link.target.url);
  EXPECT_EQ(52, second_link.start_char_index);
  EXPECT_EQ(15, second_link.char_count);
  ASSERT_EQ(1u, second_link.bounding_rects.size());
  if (using_test_fonts) {
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

TEST_P(PDFiumPageLinkTest, AnnotLinkGeneration) {
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
  if (UsingTestFonts()) {
    expected_links[0].bounding_rects[0] = {99, 436, 236, 14};
  }
  static constexpr size_t kExpectedLinkCount = std::size(expected_links);

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

TEST_P(PDFiumPageLinkTest, GetLinkTarget) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("in_doc_link_with_various_page_sizes.pdf"));
  ASSERT_EQ(3, engine->GetNumberOfPages());

  const std::vector<PDFiumPage::Link>& links = GetLinks(*engine, 0);
  ASSERT_EQ(1u, links.size());

  // Get the destination link that exists in the first page.
  PDFiumPage& first_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_LINK link = FPDFLink_GetLinkAtPoint(first_page.GetPage(), 70, 740);
  ASSERT_TRUE(link);
  FPDF_DEST dest_link = FPDFLink_GetDest(engine->doc(), link);
  ASSERT_TRUE(dest_link);

  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area = first_page.GetLinkTarget(link, &target);

  EXPECT_EQ(PDFiumPage::Area::DOCLINK_AREA, area);
  EXPECT_TRUE(target.url.empty());
  ASSERT_EQ(1, target.page);

  // Make sure the target page's size is different from the first page's. This
  // guarantees that the in-screen coordinates are calculated based on the
  // target page's dimension.
  PDFiumPage& target_page = GetPDFiumPageForTest(*engine, target.page);
  ASSERT_TRUE(target_page.available());
  ASSERT_TRUE(first_page.available());
  EXPECT_NE(GetPageSizeHelper(first_page), GetPageSizeHelper(target_page));

  ASSERT_TRUE(target.x_in_pixels.has_value());
  ASSERT_TRUE(target.y_in_pixels.has_value());
  EXPECT_FLOAT_EQ(74.666664f, target.x_in_pixels.value());
  EXPECT_FLOAT_EQ(120.f, target.y_in_pixels.value());
  EXPECT_FALSE(target.zoom.has_value());
}

// Regression test for crbug.com/1396248
TEST_P(PDFiumPageLinkTest, GetUTF8LinkTarget) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("uri_action_utf8.pdf"));
  ASSERT_EQ(1, engine->GetNumberOfPages());

  const std::vector<PDFiumPage::Link>& links = GetLinks(*engine, 0);
  ASSERT_EQ(1u, links.size());

  // Get the only link in the document.
  PDFiumPage& first_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_LINK link = FPDFLink_GetLinkAtPoint(first_page.GetPage(), 100, 100);
  ASSERT_TRUE(link);
  FPDF_DEST dest_link = FPDFLink_GetDest(engine->doc(), link);
  EXPECT_FALSE(dest_link);

  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area = first_page.GetLinkTarget(link, &target);

  EXPECT_EQ(PDFiumPage::Area::WEBLINK_AREA, area);
  EXPECT_EQ("https://site.test/hello_你好.html", target.url);
  EXPECT_EQ(-1, target.page);

  EXPECT_FALSE(target.x_in_pixels.has_value());
  EXPECT_FALSE(target.y_in_pixels.has_value());
  EXPECT_FALSE(target.zoom.has_value());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageLinkTest, testing::Bool());

using PDFiumPageImageTest = PDFiumTestBase;

TEST_P(PDFiumPageImageTest, CalculateImages) {
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

TEST_P(PDFiumPageImageTest, ImageAltText) {
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

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageImageTest, testing::Bool());

class PDFiumPageImageForOcrTest : public PDFiumPageImageTest {
 public:
  PDFiumPageImageForOcrTest() : enable_pdf_ocr_({features::kPdfOcr}) {}

  PDFiumPageImageForOcrTest(const PDFiumPageImageForOcrTest&) = delete;
  PDFiumPageImageForOcrTest& operator=(const PDFiumPageImageForOcrTest&) =
      delete;
  ~PDFiumPageImageForOcrTest() override = default;

  void SetUp() override {
    PDFiumPageImageTest::SetUp();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    PDFiumPageImageTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList enable_pdf_ocr_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

TEST_P(PDFiumPageImageForOcrTest, LowResolutionImage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("text_with_image.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(3u, page.images_.size());

  ASSERT_FALSE(page.images_[0].alt_text.empty());
  SkBitmap image_bitmap = engine->GetImageForOcr(
      /*page_index=*/0, page.images_[0].page_object_index);
  EXPECT_FALSE(image_bitmap.drawsNothing());
  EXPECT_EQ(image_bitmap.width(), 50);
  EXPECT_EQ(image_bitmap.height(), 50);

  ASSERT_TRUE(page.images_[1].alt_text.empty());
  image_bitmap = engine->GetImageForOcr(/*page_index=*/0,
                                        page.images_[1].page_object_index);
  EXPECT_FALSE(image_bitmap.drawsNothing());
  // While the scaled image size is 20x20, `image_data` has the same size as
  // the image in the PDF file, which is 50x50, and is not scaled.
  EXPECT_EQ(image_bitmap.width(), 50);
  EXPECT_EQ(image_bitmap.height(), 50);
}

TEST_P(PDFiumPageImageForOcrTest, HighResolutionImage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("big_image.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(1u, page.images_.size());

  SkBitmap image_bitmap = engine->GetImageForOcr(
      /*page_index=*/0, page.images_[0].page_object_index);
  EXPECT_FALSE(image_bitmap.drawsNothing());
  // While the original image is 5000x5000, the returned image is 267x267 as
  // OCR needs at most 300 DPI.
  EXPECT_EQ(image_bitmap.width(), 267);
  EXPECT_EQ(image_bitmap.height(), 267);
}

TEST_P(PDFiumPageImageForOcrTest, RotatedPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rotated_page.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(1u, page.images_.size());

  // This page is rotated, therefore the extracted image size is 25x100 while
  // the stored image is 100x25.
  SkBitmap image_bitmap = engine->GetImageForOcr(
      /*page_index=*/0, page.images_[0].page_object_index);
  EXPECT_EQ(image_bitmap.width(), 25);
  EXPECT_EQ(image_bitmap.height(), 100);
}

TEST_P(PDFiumPageImageForOcrTest, NonImage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("text_with_image.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  page.CalculateImages();
  ASSERT_EQ(3u, page.images_.size());
  ASSERT_EQ(1, page.images_[0].page_object_index);

  // Existing non-image object.
  SkBitmap image_bitmap = engine->GetImageForOcr(
      /*page_index=*/0, /*image_index=*/0);
  EXPECT_TRUE(image_bitmap.drawsNothing());
  EXPECT_EQ(image_bitmap.width(), 0);
  EXPECT_EQ(image_bitmap.height(), 0);

  // Out of range.
  image_bitmap = engine->GetImageForOcr(
      /*page_index=*/0, /*image_index=*/1000);
  EXPECT_TRUE(image_bitmap.drawsNothing());
  EXPECT_EQ(image_bitmap.width(), 0);
  EXPECT_EQ(image_bitmap.height(), 0);
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageImageForOcrTest, testing::Bool());

using PDFiumPageTextTest = PDFiumTestBase;

TEST_P(PDFiumPageTextTest, TextRunBounds) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("leading_trailing_spaces_per_text_run.pdf"));
  ASSERT_TRUE(engine);

  constexpr int kFirstRunStartIndex = 0;
  constexpr int kFirstRunEndIndex = 20;
  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  std::optional<AccessibilityTextRunInfo> text_run_info_1 =
      page.GetTextRunInfo(kFirstRunStartIndex);
  ASSERT_TRUE(text_run_info_1.has_value());

  const auto& actual_text_run_1 = text_run_info_1.value();
  EXPECT_EQ(21u, actual_text_run_1.len);

  EXPECT_TRUE(
      base::IsUnicodeWhitespace(page.GetCharUnicode(kFirstRunStartIndex)));
  gfx::RectF text_run_bounds = actual_text_run_1.bounds;
  EXPECT_TRUE(
      text_run_bounds.Contains(page.GetCharBounds(kFirstRunStartIndex)));

  // Last non-space character should fall in the bounding box of the text run.
  // Text run looks like this:
  // " Hello, world! \r\n "<17 characters><first Tj>
  // " \r\n "<4 characters><second Tj>
  // " "<1 character><third Tj starting spaces>
  // Finally generated text run: " Hello, world! \r\n \r\n "
  constexpr int kFirstRunLastNonSpaceCharIndex = 13;
  EXPECT_FALSE(base::IsUnicodeWhitespace(
      page.GetCharUnicode(kFirstRunLastNonSpaceCharIndex)));
  EXPECT_TRUE(text_run_bounds.Contains(
      page.GetCharBounds(kFirstRunLastNonSpaceCharIndex)));

  EXPECT_TRUE(
      base::IsUnicodeWhitespace(page.GetCharUnicode(kFirstRunEndIndex)));
  gfx::RectF end_char_rect = page.GetCharBounds(kFirstRunEndIndex);
  EXPECT_FALSE(text_run_bounds.Contains(end_char_rect));
  // Equals to the length of the previous text run.
  constexpr int kSecondRunStartIndex = 21;
  constexpr int kSecondRunEndIndex = 36;
  // Test the properties of second text run.
  // Note: The leading spaces in second text run are accounted for in the end
  // of first text run. Hence we won't see a space leading the second text run.
  std::optional<AccessibilityTextRunInfo> text_run_info_2 =
      page.GetTextRunInfo(kSecondRunStartIndex);
  ASSERT_TRUE(text_run_info_2.has_value());

  const auto& actual_text_run_2 = text_run_info_2.value();
  EXPECT_EQ(16u, actual_text_run_2.len);

  EXPECT_FALSE(
      base::IsUnicodeWhitespace(page.GetCharUnicode(kSecondRunStartIndex)));
  text_run_bounds = actual_text_run_2.bounds;
  EXPECT_TRUE(
      text_run_bounds.Contains(page.GetCharBounds(kSecondRunStartIndex)));

  // Last non-space character should fall in the bounding box of the text run.
  // Text run looks like this:
  // "Goodbye, world! "<19 characters><first Tj>
  // Finally generated text run: "Goodbye, world! "
  constexpr int kSecondRunLastNonSpaceCharIndex = 35;
  EXPECT_FALSE(base::IsUnicodeWhitespace(
      page.GetCharUnicode(kSecondRunLastNonSpaceCharIndex)));
  EXPECT_TRUE(text_run_bounds.Contains(
      page.GetCharBounds(kSecondRunLastNonSpaceCharIndex)));

  EXPECT_TRUE(
      base::IsUnicodeWhitespace(page.GetCharUnicode(kSecondRunEndIndex)));
  EXPECT_FALSE(
      text_run_bounds.Contains(page.GetCharBounds(kSecondRunEndIndex)));
}

TEST_P(PDFiumPageTextTest, GetTextRunInfo) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);

  int current_char_index = 0;

  AccessibilityTextStyleInfo expected_style_1 = {
      "Times-Roman",
      0,
      AccessibilityTextRenderMode::kFill,
      12,
      0xff000000,
      0xff000000,
      false,
      false};
  AccessibilityTextStyleInfo expected_style_2 = {
      "Helvetica", 0,          AccessibilityTextRenderMode::kFill,
      16,          0xff000000, 0xff000000,
      false,       false};
  // The links span from [7, 22], [52, 66] and [92, 108] with 16, 15 and 17
  // text run lengths respectively. There are text runs preceding and
  // succeeding them.
  AccessibilityTextRunInfo expected_text_runs[] = {
      {7, gfx::RectF(26.666666f, 189.333333f, 38.666672f, 13.333344f),
       AccessibilityTextDirection::kLeftToRight, expected_style_1},
      {16, gfx::RectF(70.666664f, 189.333333f, 108.0f, 14.666672f),
       AccessibilityTextDirection::kLeftToRight, expected_style_1},
      {20, gfx::RectF(181.333333f, 189.333333f, 117.333333f, 14.666672f),
       AccessibilityTextDirection::kLeftToRight, expected_style_1},
      {9, gfx::RectF(28.0f, 117.33334f, 89.333328f, 20.0f),
       AccessibilityTextDirection::kLeftToRight, expected_style_2},
      {15, gfx::RectF(126.66666f, 117.33334f, 137.33334f, 20.0f),
       AccessibilityTextDirection::kLeftToRight, expected_style_2},
      {20, gfx::RectF(266.66666f, 118.66666f, 169.33334f, 18.666664f),
       AccessibilityTextDirection::kLeftToRight, expected_style_2},
      {5, gfx::RectF(28.0f, 65.333336f, 40.0f, 18.666664f),
       AccessibilityTextDirection::kLeftToRight, expected_style_2},
      {17, gfx::RectF(77.333336f, 64.0f, 160.0f, 20.0f),
       AccessibilityTextDirection::kLeftToRight, expected_style_2}};

  if (UsingTestFonts()) {
    expected_text_runs[4].bounds =
        gfx::RectF(126.66666f, 117.33334f, 137.33334f, 21.33334f);
    expected_text_runs[5].bounds =
        gfx::RectF(266.66666f, 118.66666f, 170.66666f, 20.0f);
    expected_text_runs[7].bounds =
        gfx::RectF(77.333336f, 64.0f, 160.0f, 21.33333f);
  }

  // Test negative char index returns nullopt
  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  std::optional<AccessibilityTextRunInfo> text_run_info_result =
      page.GetTextRunInfo(-1);
  ASSERT_FALSE(text_run_info_result.has_value());

  // Test valid char index returns expected text run info and expected text
  // style info
  for (const auto& expected_text_run : expected_text_runs) {
    text_run_info_result = page.GetTextRunInfo(current_char_index);
    ASSERT_TRUE(text_run_info_result.has_value());
    const auto& actual_text_run = text_run_info_result.value();
    CompareTextRuns(expected_text_run, actual_text_run);
    current_char_index += actual_text_run.len;
  }

  // Test char index outside char range returns nullopt
  EXPECT_EQ(page.GetCharCount(), current_char_index);
  text_run_info_result = page.GetTextRunInfo(current_char_index);
  ASSERT_FALSE(text_run_info_result.has_value());
}

TEST_P(PDFiumPageTextTest, HighlightTextRunInfo) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("highlights.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  // Highlights span across text run indices 0, 2 and 3.
  static const AccessibilityTextStyleInfo kExpectedStyle = {
      "Helvetica", 0,          AccessibilityTextRenderMode::kFill,
      16,          0xff000000, 0xff000000,
      false,       false};
  AccessibilityTextRunInfo expected_text_runs[] = {
      {5, gfx::RectF(1.3333334f, 198.66667f, 46.666668f, 14.666672f),
       AccessibilityTextDirection::kLeftToRight, kExpectedStyle},
      {7, gfx::RectF(50.666668f, 198.66667f, 47.999996f, 17.333328f),
       AccessibilityTextDirection::kLeftToRight, kExpectedStyle},
      {7, gfx::RectF(106.66666f, 198.66667f, 73.333336f, 18.666672f),
       AccessibilityTextDirection::kLeftToRight, kExpectedStyle},
      {2, gfx::RectF(181.33333f, 202.66667f, 16.0f, 14.66667f),
       AccessibilityTextDirection::kNone, kExpectedStyle},
      {2, gfx::RectF(198.66667f, 202.66667f, 21.333328f, 10.666672f),
       AccessibilityTextDirection::kLeftToRight, kExpectedStyle}};

  if (UsingTestFonts()) {
    expected_text_runs[2].bounds =
        gfx::RectF(106.66666f, 198.66667f, 73.333336f, 19.999985f);
    expected_text_runs[4].bounds =
        gfx::RectF(198.66667f, 201.33333f, 21.333328f, 12.000015f);
  }

  PDFiumPage& page = GetPDFiumPageForTest(*engine, 0);
  int current_char_index = 0;
  for (const auto& expected_text_run : expected_text_runs) {
    std::optional<AccessibilityTextRunInfo> text_run_info_result =
        page.GetTextRunInfo(current_char_index);
    ASSERT_TRUE(text_run_info_result.has_value());
    const auto& actual_text_run = text_run_info_result.value();
    CompareTextRuns(expected_text_run, actual_text_run);
    current_char_index += actual_text_run.len;
  }
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageTextTest, testing::Bool());

using PDFiumPageHighlightTest = PDFiumTestBase;

TEST_P(PDFiumPageHighlightTest, PopulateHighlights) {
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
  ASSERT_EQ(std::size(kExpectedHighlights), page.highlights_.size());

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

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageHighlightTest, testing::Bool());

using PDFiumPageTextFieldTest = PDFiumTestBase;

TEST_P(PDFiumPageTextFieldTest, PopulateTextFields) {
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
  ASSERT_EQ(std::size(kExpectedTextFields), text_fields_count);

  for (size_t i = 0; i < text_fields_count; ++i) {
    EXPECT_EQ(kExpectedTextFields[i].name, page.text_fields_[i].name);
    EXPECT_EQ(kExpectedTextFields[i].value, page.text_fields_[i].value);
    EXPECT_EQ(kExpectedTextFields[i].bounding_rect,
              page.text_fields_[i].bounding_rect);
    EXPECT_EQ(kExpectedTextFields[i].flags, page.text_fields_[i].flags);
  }
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageTextFieldTest, testing::Bool());

using PDFiumPageChoiceFieldTest = PDFiumTestBase;

TEST_P(PDFiumPageChoiceFieldTest, PopulateChoiceFields) {
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
  ASSERT_EQ(std::size(kExpectedChoiceFields), choice_fields_count);

  for (size_t i = 0; i < choice_fields_count; ++i) {
    EXPECT_EQ(kExpectedChoiceFields[i].name, page.choice_fields_[i].name);
    size_t choice_field_options_count = page.choice_fields_[i].options.size();
    ASSERT_EQ(std::size(kExpectedChoiceFields[i].options),
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

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageChoiceFieldTest, testing::Bool());

using PDFiumPageButtonTest = PDFiumTestBase;

TEST_P(PDFiumPageButtonTest, PopulateButtons) {
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
  ASSERT_EQ(std::size(kExpectedButtons), buttons_count);

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

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageButtonTest, testing::Bool());

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
    EXPECT_EQ(expected_thumbnail_size, thumbnail.image_size());
    EXPECT_EQ(device_pixel_ratio, thumbnail.device_pixel_ratio());

    auto image_info =
        SkImageInfo::Make(gfx::SizeToSkISize(thumbnail.image_size()),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    int stride = thumbnail.stride();
    ASSERT_GT(stride, 0);
    ASSERT_EQ(image_info.minRowBytes(), static_cast<size_t>(stride));
    std::vector<uint8_t> data = thumbnail.TakeData();
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(
        SkPixmap(image_info, data.data(), image_info.minRowBytes()));
    ASSERT_TRUE(image);

    base::FilePath expectation_png_file_path =
        GetThumbnailTestData(expectation_file_prefix, page_index,
                             device_pixel_ratio, /*use_skia=*/GetParam());

    EXPECT_TRUE(MatchesPngFile(image.get(), expectation_png_file_path));
  }
};

TEST_P(PDFiumPageThumbnailTest, GenerateThumbnail) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("variable_page_sizes.pdf"));
  ASSERT_EQ(7, engine->GetNumberOfPages());

#if defined(ARCH_CPU_ARM64)
  std::string file_name =
      GetParam() ? "variable_page_sizes_arm64" : "variable_page_sizes";
#else
  std::string file_name = "variable_page_sizes";
#endif
  for (const auto& params : kGenerateThumbnailTestParams) {
    TestGenerateThumbnail(*engine, params.page_index, params.device_pixel_ratio,
                          params.expected_thumbnail_size, file_name);
  }
}

// For crbug.com/1248455
TEST_P(PDFiumPageThumbnailTest, GenerateThumbnailForAnnotation) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("signature_widget.pdf"));
  TestGenerateThumbnail(*engine, /*page_index=*/0, /*device_pixel_ratio=*/1,
                        /*expected_thumbnail_size=*/{140, 140},
                        "signature_widget");
  TestGenerateThumbnail(*engine, /*page_index=*/0, /*device_pixel_ratio=*/2,
                        /*expected_thumbnail_size=*/{255, 255},
                        "signature_widget");
}

#if BUILDFLAG(ENABLE_PDF_INK2)
TEST_P(PDFiumPageThumbnailTest, GetThumbnailSize) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("variable_page_sizes.pdf"));
  ASSERT_EQ(7, engine->GetNumberOfPages());

  for (const auto& params : kGenerateThumbnailTestParams) {
    EXPECT_EQ(
        params.expected_thumbnail_size,
        engine->GetThumbnailSize(params.page_index, params.device_pixel_ratio));
  }
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

INSTANTIATE_TEST_SUITE_P(All, PDFiumPageThumbnailTest, testing::Bool());

}  // namespace chrome_pdf
