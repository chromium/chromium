// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "build/chromeos_buildflags.h"
#include "pdf/pdf.h"
#include "pdf/pdf_watermark_overlayer.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_text.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdf_progressive_searchifier.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace chrome_pdf {

namespace {

class ScopedLibraryInitializer {
 public:
  ScopedLibraryInitializer() {
    InitializeSDK(/*enable_v8=*/true, /*use_skia=*/false,
                  FontMappingMode::kNoMapping);
  }
  ~ScopedLibraryInitializer() { ShutdownSDK(); }
};

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

// Returns all characters in the page.
std::string GetText(base::span<const uint8_t> pdf, int page_index) {
  ScopedFPDFDocument document = LoadPdfData(pdf);
  CHECK(document);
  ScopedFPDFPage page(FPDF_LoadPage(document.get(), page_index));
  CHECK(page);
  ScopedFPDFTextPage text_page(FPDFText_LoadPage(page.get()));
  CHECK(text_page);
  int char_count = FPDFText_CountChars(text_page.get());
  CHECK_GE(char_count, 0);
  std::u16string text;
  text.reserve(char_count);
  for (int i = 0; i < char_count; ++i) {
    unsigned int char_code = FPDFText_GetUnicode(text_page.get(), i);
    text += static_cast<char16_t>(char_code);
  }
  return base::UTF16ToUTF8(text);
}

std::vector<gfx::RectF> GetTextPositions(base::span<const uint8_t> pdf,
                                         int page_index) {
  ScopedFPDFDocument document = LoadPdfData(pdf);
  CHECK(document);
  ScopedFPDFPage page(FPDF_LoadPage(document.get(), page_index));
  CHECK(page);
  ScopedFPDFTextPage text_page(FPDFText_LoadPage(page.get()));
  CHECK(text_page);
  int char_count = FPDFText_CountChars(text_page.get());
  CHECK_GE(char_count, 0);
  std::vector<gfx::RectF> positions;
  positions.reserve(char_count);
  for (int i = 0; i < char_count; ++i) {
    positions.push_back(
        GetTextCharBox(text_page.get(), i).value().AsGfxRectF());
  }
  return positions;
}
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

class PDFiumEngineExportsTest : public testing::Test {
 public:
  PDFiumEngineExportsTest() = default;
  PDFiumEngineExportsTest(const PDFiumEngineExportsTest&) = delete;
  PDFiumEngineExportsTest& operator=(const PDFiumEngineExportsTest&) = delete;
  ~PDFiumEngineExportsTest() override = default;

 protected:
  void SetUp() override {
    pdf_data_dir_ = base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
                        .Append(FILE_PATH_LITERAL("pdf"))
                        .Append(FILE_PATH_LITERAL("test"))
                        .Append(FILE_PATH_LITERAL("data"));
  }

  const base::FilePath& pdf_data_dir() const { return pdf_data_dir_; }

  std::vector<uint8_t> ReadTestData(
      base::FilePath::StringViewType filename) const {
    base::FilePath pdf_path = pdf_data_dir_.Append(filename);
    return base::ReadFileToBytes(pdf_path).value_or(std::vector<uint8_t>());
  }

 private:
  base::FilePath pdf_data_dir_;
};

}  // namespace

TEST_F(PDFiumEngineExportsTest, GetPDFDocInfo) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("hello_world2.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), nullptr, nullptr));

  int page_count;
  float max_page_width;
  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), &page_count, &max_page_width));
  EXPECT_EQ(2, page_count);
  EXPECT_DOUBLE_EQ(200.0, max_page_width);
}

TEST_F(PDFiumEngineExportsTest, GetPDFPageSizeByIndex) {
  // TODO(thestig): Use a better PDF for this test, as hello_world2.pdf's page
  // dimensions are uninteresting.
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("hello_world2.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), &page_count, nullptr));
  ASSERT_EQ(2, page_count);
  for (int page_index = 0; page_index < page_count; ++page_index) {
    std::optional<gfx::SizeF> page_size =
        GetPDFPageSizeByIndex(pdf_data.value(), page_index);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(gfx::SizeF(200, 200), page_size.value());
  }
}

TEST_F(PDFiumEngineExportsTest, ConvertPdfPagesToNupPdf) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("rectangles.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  std::vector<base::span<const uint8_t>> pdf_buffers;
  std::vector<uint8_t> output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 1, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());

  pdf_buffers.push_back(pdf_data.value());
  pdf_buffers.push_back(pdf_data.value());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 0, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 0));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(300, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 400, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  ASSERT_GT(output_pdf_buffer.size(), 0U);

  base::span<const uint8_t> output_pdf_span = base::span(output_pdf_buffer);
  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf_span, &page_count, nullptr));
  ASSERT_EQ(1, page_count);

  std::optional<gfx::SizeF> page_size =
      GetPDFPageSizeByIndex(output_pdf_span, 0);
  ASSERT_TRUE(page_size.has_value());
  EXPECT_EQ(gfx::SizeF(792, 612), page_size.value());
}

TEST_F(PDFiumEngineExportsTest, ConvertPdfDocumentToNupPdf) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  std::vector<uint8_t> output_pdf_buffer = ConvertPdfDocumentToNupPdf(
      {}, 1, gfx::Size(612, 792), gfx::Rect(32, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());

  output_pdf_buffer = ConvertPdfDocumentToNupPdf(
      pdf_data.value(), 4, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  ASSERT_GT(output_pdf_buffer.size(), 0U);

  base::span<const uint8_t> output_pdf_span = base::span(output_pdf_buffer);
  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf_span, &page_count, nullptr));
  ASSERT_EQ(2, page_count);
  for (int page_index = 0; page_index < page_count; ++page_index) {
    std::optional<gfx::SizeF> page_size =
        GetPDFPageSizeByIndex(output_pdf_span, page_index);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(gfx::SizeF(612, 792), page_size.value());
  }
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerBasic) {
  std::vector<uint8_t> pdf_buffer =
      ReadTestData(FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::vector<uint8_t> output_pdf;
  {
    // `overlayer` must be destroyed before calling `GetPDFDocInfo()` below,
    // to avoid overlapping PDFium SDK initializers.
    std::unique_ptr<PdfWatermarkOverlayer> overlayer =
        CreatePdfWatermarkOverlayer(pdf_buffer);
    ASSERT_TRUE(overlayer);

    std::vector<gfx::SizeF> page_sizes = overlayer->GetPageSizes();
    ASSERT_EQ(1u, page_sizes.size());
    EXPECT_EQ(gfx::SizeF(200, 300), page_sizes[0]);

    output_pdf = overlayer->OverlayPages(pdf_buffer);
    ASSERT_FALSE(output_pdf.empty());
  }

  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf, &page_count, nullptr));
  EXPECT_EQ(1, page_count);

  std::optional<gfx::SizeF> output_size = GetPDFPageSizeByIndex(output_pdf, 0);
  ASSERT_TRUE(output_size.has_value());
  EXPECT_EQ(gfx::SizeF(200, 300), output_size.value());
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerMultiPageSuccess) {
  std::vector<uint8_t> pdf_buffer = ReadTestData(
      FILE_PATH_LITERAL("multi_page_hello_world_with_empty_page.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::vector<uint8_t> output_pdf;
  {
    // `overlayer` must be destroyed before calling `GetPDFDocInfo()` below,
    // to avoid overlapping PDFium SDK initializers.
    std::unique_ptr<PdfWatermarkOverlayer> overlayer =
        CreatePdfWatermarkOverlayer(pdf_buffer);
    ASSERT_TRUE(overlayer);

    std::vector<gfx::SizeF> page_sizes = overlayer->GetPageSizes();
    ASSERT_EQ(3u, page_sizes.size());

    output_pdf = overlayer->OverlayPages(pdf_buffer);
    ASSERT_FALSE(output_pdf.empty());
  }

  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf, &page_count, nullptr));
  EXPECT_EQ(3, page_count);
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerMismatchedPages) {
  std::vector<uint8_t> pdf_buffer =
      ReadTestData(FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::vector<uint8_t> multi_page_buffer = ReadTestData(
      FILE_PATH_LITERAL("multi_page_hello_world_with_empty_page.pdf"));
  ASSERT_FALSE(multi_page_buffer.empty());

  std::unique_ptr<PdfWatermarkOverlayer> overlayer =
      CreatePdfWatermarkOverlayer(pdf_buffer);
  ASSERT_TRUE(overlayer);

  std::vector<uint8_t> output_pdf =
      overlayer->OverlayPages(multi_page_buffer);
  EXPECT_TRUE(output_pdf.empty());
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerMismatchedDimensions) {
  std::vector<uint8_t> pdf_buffer =
      ReadTestData(FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::vector<uint8_t> blank_buffer =
      ReadTestData(FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_FALSE(blank_buffer.empty());

  std::unique_ptr<PdfWatermarkOverlayer> overlayer =
      CreatePdfWatermarkOverlayer(pdf_buffer);
  ASSERT_TRUE(overlayer);

  // Both PDFs have 1 page, but rectangles.pdf is 200x300 and blank.pdf is
  // 200x200.
  std::vector<uint8_t> output_pdf = overlayer->OverlayPages(blank_buffer);
  EXPECT_TRUE(output_pdf.empty());
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerInvalidBuffer) {
  std::vector<uint8_t> empty_buffer;
  EXPECT_FALSE(CreatePdfWatermarkOverlayer(empty_buffer));

  constexpr uint8_t kCorruptData[] = "Not a valid PDF header";
  EXPECT_FALSE(CreatePdfWatermarkOverlayer(kCorruptData));
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerInvalidOverlay) {
  std::vector<uint8_t> pdf_buffer =
      ReadTestData(FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::unique_ptr<PdfWatermarkOverlayer> overlayer =
      CreatePdfWatermarkOverlayer(pdf_buffer);
  ASSERT_TRUE(overlayer);

  // 1. Empty overlay buffer
  EXPECT_TRUE(overlayer->OverlayPages({}).empty());

  // 2. Corrupt overlay buffer
  constexpr uint8_t kCorruptOverlay[] = "Not a valid PDF overlay stream";
  EXPECT_TRUE(overlayer->OverlayPages(kCorruptOverlay).empty());
}

TEST_F(PDFiumEngineExportsTest, PdfWatermarkOverlayerBadPage) {
  std::vector<uint8_t> pdf_buffer =
      ReadTestData(FILE_PATH_LITERAL("bad_page.pdf"));
  ASSERT_FALSE(pdf_buffer.empty());

  std::unique_ptr<PdfWatermarkOverlayer> overlayer =
      CreatePdfWatermarkOverlayer(pdf_buffer);
  ASSERT_TRUE(overlayer);

  // GetPageSizes() should fail on bad_page.pdf and return {}
  EXPECT_TRUE(overlayer->GetPageSizes().empty());
}

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
constexpr char kExpectedText[] = "Hello World! 你好！🙂";

TEST_F(PDFiumEngineExportsTest, Searchify) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("image_alt_text.pdf"));
  std::optional<std::vector<uint8_t>> pdf_buffer =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_buffer.has_value());

  base::MockCallback<base::RepeatingCallback<
      screen_ai::mojom::VisualAnnotationPtr(const SkBitmap&)>>
      perform_ocr_callback;
  // The PDF has 3 images, but one of them has a matrix that results in an image
  // with size 0, and its image cannot be extracted.
  EXPECT_CALL(perform_ocr_callback, Run)
      .Times(2)
      .WillRepeatedly([](const SkBitmap& bitmap) {
        CHECK(!bitmap.empty());
        auto annotation = screen_ai::mojom::VisualAnnotation::New();
        auto line_box = screen_ai::mojom::LineBox::New();
        line_box->bounding_box = gfx::Rect(0, 0, 100, 100);
        line_box->bounding_box_angle = 0;
        auto word_box = screen_ai::mojom::WordBox::New();
        word_box->word = kExpectedText;
        word_box->bounding_box = gfx::Rect(0, 0, 100, 100);
        word_box->bounding_box_angle = 0;
        line_box->words.push_back(std::move(word_box));
        annotation->lines.push_back(std::move(line_box));
        return annotation;
      });
  {
    ScopedLibraryInitializer initializer;
    EXPECT_THAT(GetText(*pdf_buffer, 0),
                testing::Not(testing::HasSubstr(kExpectedText)));
  }
  std::vector<uint8_t> output_pdf_buffer =
      Searchify(*pdf_buffer, perform_ocr_callback.Get());
  ASSERT_FALSE(output_pdf_buffer.empty());
  {
    ScopedLibraryInitializer initializer;
    EXPECT_THAT(GetText(output_pdf_buffer, 0),
                testing::HasSubstr(kExpectedText));
  }
}

TEST_F(PDFiumEngineExportsTest, SearchifyBroken) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("image_alt_text.pdf"));
  std::optional<std::vector<uint8_t>> pdf_buffer =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_buffer.has_value());

  auto broken_perform_ocr_callback =
      base::BindRepeating([](const SkBitmap& bitmap) {
        auto annotation = screen_ai::mojom::VisualAnnotationPtr();
        // Simulate the scenarios that fail to set the value of `annotation`.
        CHECK(!annotation);
        return annotation;
      });
  std::vector<uint8_t> output_pdf_buffer =
      Searchify(*pdf_buffer, std::move(broken_perform_ocr_callback));
  EXPECT_TRUE(output_pdf_buffer.empty());
}

TEST_F(PDFiumEngineExportsTest, SearchifyBigImage) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("big_image.pdf"));
  std::optional<std::vector<uint8_t>> pdf_buffer =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_buffer.has_value());

  base::MockCallback<base::RepeatingCallback<
      screen_ai::mojom::VisualAnnotationPtr(const SkBitmap&)>>
      perform_ocr_callback;
  EXPECT_CALL(perform_ocr_callback, Run).WillOnce([](const SkBitmap& bitmap) {
    // The returned image is 2048x2048 as OCR needs at most 2048x2048 pixels.
    EXPECT_EQ(2048, bitmap.width());
    EXPECT_EQ(2048, bitmap.height());
    auto annotation = screen_ai::mojom::VisualAnnotation::New();

    constexpr gfx::Rect kRect1(0, 230, 77, 38);
    auto line_box1 = screen_ai::mojom::LineBox::New();
    line_box1->bounding_box = kRect1;
    line_box1->bounding_box_angle = 0;
    auto word_box1 = screen_ai::mojom::WordBox::New();
    word_box1->word = "a";
    word_box1->bounding_box = kRect1;
    word_box1->bounding_box_angle = 0;
    line_box1->words.push_back(std::move(word_box1));
    annotation->lines.push_back(std::move(line_box1));

    constexpr gfx::Rect kRect2(1534, 1611, 514, 437);
    auto line_box2 = screen_ai::mojom::LineBox::New();
    line_box2->bounding_box = kRect2;
    line_box2->bounding_box_angle = 0;
    auto word_box2 = screen_ai::mojom::WordBox::New();
    word_box2->word = "b";
    word_box2->bounding_box = kRect2;
    word_box2->bounding_box_angle = 0;
    line_box2->words.push_back(std::move(word_box2));
    annotation->lines.push_back(std::move(line_box2));
    return annotation;
  });
  {
    ScopedLibraryInitializer initializer;
    EXPECT_TRUE(GetTextPositions(*pdf_buffer, 0).empty());
  }
  std::vector<uint8_t> output_pdf_buffer =
      Searchify(*pdf_buffer, perform_ocr_callback.Get());
  ASSERT_FALSE(output_pdf_buffer.empty());
  {
    constexpr float kFloatTolerance = 0.0001f;
    ScopedLibraryInitializer initializer;
    // The middle 1 position is for auto-generated "\r\n".
    const std::vector<gfx::RectF> positions =
        GetTextPositions(output_pdf_buffer, 0);
    ASSERT_EQ(3u, positions.size());
    EXPECT_RECTF_NEAR(gfx::RectF(0, 55.625f, 2.40625f, 1.1875f), positions[0],
                      kFloatTolerance);
    EXPECT_TRUE(positions[1].IsEmpty());
    EXPECT_RECTF_NEAR(gfx::RectF(47.9375f, 0, 16.0625f, 13.6562f), positions[2],
                      kFloatTolerance);
  }
}

TEST_F(PDFiumEngineExportsTest, PdfProgressiveSearchifier) {
  std::unique_ptr<PdfProgressiveSearchifier> progressive_searchifier =
      CreateProgressiveSearchifier();

  std::vector<uint8_t> zero_page_pdf = progressive_searchifier->Save();
  ASSERT_GT(zero_page_pdf.size(), 0U);

  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  SkBitmap bitmap_1x1;
  bitmap_1x1.allocN32Pixels(1, 1);
  SkBitmap bitmap_100x100;
  bitmap_100x100.allocN32Pixels(100, 100);

  progressive_searchifier->AddPage(bitmap_1x1, 0, annotation->Clone());
  // Pages: [1x1]
  std::vector<uint8_t> one_page_pdf = progressive_searchifier->Save();
  ASSERT_GT(one_page_pdf.size(), 0U);

  progressive_searchifier->AddPage(bitmap_100x100, 1, annotation->Clone());
  // Pages: [1x1, 100x100]
  std::vector<uint8_t> two_page_pdf = progressive_searchifier->Save();
  ASSERT_GT(two_page_pdf.size(), 0U);

  progressive_searchifier->AddPage(bitmap_1x1, 0, annotation->Clone());
  progressive_searchifier->AddPage(bitmap_1x1, 2, annotation->Clone());
  // Pages: [1x1, 100x100, 1x1]
  std::vector<uint8_t> three_page_pdf_replaced =
      progressive_searchifier->Save();
  ASSERT_GT(three_page_pdf_replaced.size(), 0U);

  progressive_searchifier->DeletePage(1);
  progressive_searchifier->DeletePage(1);
  // Pages: [1x1]
  std::vector<uint8_t> one_page_pdf_deleted = progressive_searchifier->Save();
  ASSERT_GT(one_page_pdf_deleted.size(), 0U);

  progressive_searchifier.reset();

  int page_count;
  float max_page_width;
  ASSERT_TRUE(GetPDFDocInfo(zero_page_pdf, &page_count, &max_page_width));
  EXPECT_EQ(page_count, 0);
  EXPECT_EQ(max_page_width, 0);

  ASSERT_TRUE(GetPDFDocInfo(one_page_pdf, &page_count, &max_page_width));
  EXPECT_EQ(page_count, 1);
  EXPECT_EQ(max_page_width, bitmap_1x1.width());

  ASSERT_TRUE(GetPDFDocInfo(two_page_pdf, &page_count, &max_page_width));
  EXPECT_EQ(page_count, 2);
  EXPECT_EQ(max_page_width, bitmap_100x100.width());

  ASSERT_TRUE(
      GetPDFDocInfo(three_page_pdf_replaced, &page_count, &max_page_width));
  EXPECT_EQ(page_count, 3);
  EXPECT_EQ(max_page_width, bitmap_100x100.width());

  ASSERT_TRUE(
      GetPDFDocInfo(one_page_pdf_deleted, &page_count, &max_page_width));
  EXPECT_EQ(page_count, 1);
  EXPECT_EQ(max_page_width, bitmap_1x1.width());
}

TEST_F(PDFiumEngineExportsTest, PdfProgressiveSearchifierText) {
  std::unique_ptr<PdfProgressiveSearchifier> progressive_searchifier =
      CreateProgressiveSearchifier();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line_box = screen_ai::mojom::LineBox::New();
  line_box->bounding_box = gfx::Rect(0, 0, 100, 100);
  line_box->bounding_box_angle = 0;
  auto word_box = screen_ai::mojom::WordBox::New();
  word_box->word = kExpectedText;
  word_box->bounding_box = gfx::Rect(0, 0, 100, 100);
  word_box->bounding_box_angle = 0;
  line_box->words.push_back(std::move(word_box));
  annotation->lines.push_back(std::move(line_box));

  progressive_searchifier->AddPage(bitmap, 0, std::move(annotation));
  std::vector<uint8_t> pdf = progressive_searchifier->Save();
  progressive_searchifier.reset();

  {
    ScopedLibraryInitializer initializer;
    EXPECT_THAT(GetText(pdf, 0), testing::HasSubstr(kExpectedText));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace chrome_pdf
