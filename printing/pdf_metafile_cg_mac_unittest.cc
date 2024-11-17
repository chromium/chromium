// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cg_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

namespace {

base::FilePath GetPdfTestData(const base::FilePath::StringType& filename) {
  base::FilePath root_path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path)) {
    return base::FilePath();
  }
  return root_path.Append("pdf").Append("test").Append("data").Append(filename);
}

base::FilePath GetPrintingTestData(const base::FilePath::StringType& filename) {
  base::FilePath root_path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path)) {
    return base::FilePath();
  }
  return root_path.Append("printing")
      .Append("test")
      .Append("data")
      .Append("pdf_cg")
      .Append(filename);
}

std::unique_ptr<PdfMetafileCg> GetPdfMetafile(
    const base::FilePath::StringType& pdf_filename) {
  // Get test data.
  base::FilePath pdf_file = GetPdfTestData(pdf_filename);
  if (pdf_file.empty())
    return nullptr;

  std::string pdf_data;
  if (!base::ReadFileToString(pdf_file, &pdf_data))
    return nullptr;

  // Initialize and check metafile.
  auto pdf_cg = std::make_unique<PdfMetafileCg>();
  if (!pdf_cg->InitFromData(base::as_bytes(base::make_span(pdf_data))))
    return nullptr;
  return pdf_cg;
}

void RenderedPdfSha1(const base::FilePath::StringType& pdf_filename,
                     size_t page_number,
                     const gfx::Rect& expected_page_bounds,
                     const gfx::Size& dest_size,
                     bool autorotate,
                     bool fit_to_page,
                     base::SHA1Digest* rendered_hash) {
  // Initialize and verify the metafile.
  std::unique_ptr<PdfMetafileCg> pdf_cg = GetPdfMetafile(pdf_filename);
  ASSERT_TRUE(pdf_cg);
  ASSERT_LE(page_number, pdf_cg->GetPageCount());
  const gfx::Rect bounds = pdf_cg->GetPageBounds(page_number);
  ASSERT_EQ(expected_page_bounds, bounds);

  // Set up rendering context.
  constexpr size_t kBitsPerComponent = 8;
  constexpr size_t kBytesPerPixel = 4;
  const size_t kStride = dest_size.width() * kBytesPerPixel;
  std::vector<uint8_t> rendered_bitmap(dest_size.height() * kStride);
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      rendered_bitmap.data(), dest_size.width(), dest_size.height(),
      kBitsPerComponent, kStride, color_space.get(),
      uint32_t{kCGImageAlphaPremultipliedFirst} | kCGBitmapByteOrder32Little));

  // Render using metafile and calculate the output hash.
  ASSERT_TRUE(pdf_cg->RenderPage(page_number, context.get(),
                                 gfx::Rect(dest_size).ToCGRect(), autorotate,
                                 fit_to_page));
  *rendered_hash = base::SHA1Hash(rendered_bitmap);
}

void ExpectedPngSha1(const base::FilePath::StringType& expected_png_filename,
                     const gfx::Size& expected_png_size,
                     base::SHA1Digest* expected_hash) {
  base::FilePath expected_png_file = GetPrintingTestData(expected_png_filename);
  ASSERT_FALSE(expected_png_file.empty());
  std::optional<std::vector<uint8_t>> expected_png_data =
      base::ReadFileToBytes(expected_png_file);
  ASSERT_TRUE(expected_png_data);

  // Decode expected PNG and calculate the output hash.
  std::optional<gfx::PNGCodec::DecodeOutput> decoded = gfx::PNGCodec::Decode(
      expected_png_data.value(), gfx::PNGCodec::FORMAT_BGRA);
  ASSERT_TRUE(decoded);
  ASSERT_EQ(expected_png_size.width(), decoded->width);
  ASSERT_EQ(expected_png_size.height(), decoded->height);
  *expected_hash = base::SHA1Hash(decoded->output);
}

void TestRenderPageWithTransformParams(
    const base::FilePath::StringType& pdf_filename,
    size_t page_number,
    const gfx::Rect& expected_page_bounds,
    const base::FilePath::StringType& expected_png_filename,
    const gfx::Size& dest_size,
    bool autorotate,
    bool fit_to_page) {
  base::SHA1Digest rendered_hash;
  RenderedPdfSha1(pdf_filename, page_number, expected_page_bounds, dest_size,
                  autorotate, fit_to_page, &rendered_hash);
  base::SHA1Digest expected_hash;
  ExpectedPngSha1(expected_png_filename, dest_size, &expected_hash);

  // Make sure the hashes match.
  EXPECT_EQ(expected_hash, rendered_hash);
}

void TestRenderPage(const base::FilePath::StringType& pdf_filename,
                    size_t page_number,
                    const gfx::Rect& expected_page_bounds,
                    const base::FilePath::StringType& expected_png_filename,
                    const gfx::Size& dest_size) {
  TestRenderPageWithTransformParams(
      pdf_filename, page_number, expected_page_bounds, expected_png_filename,
      dest_size, /*autorotate=*/true, /*fit_to_page=*/false);
}

}  // namespace

TEST(PdfMetafileCgTest, Pdf) {
  // Test in-renderer constructor.
  PdfMetafileCg pdf;
  EXPECT_TRUE(pdf.Init());
  EXPECT_TRUE(pdf.context());

  // Render page 1.
  constexpr gfx::Rect kRect1(10, 10, 520, 700);
  constexpr gfx::Size kSize1(540, 720);
  pdf.StartPage(kSize1, kRect1, 1.25, mojom::PageOrientation::kUpright);
  pdf.FinishPage();

  // Render page 2.
  constexpr gfx::Rect kRect2(10, 10, 520, 700);
  constexpr gfx::Size kSize2(720, 540);
  pdf.StartPage(kSize2, kRect2, 2.0, mojom::PageOrientation::kUpright);
  pdf.FinishPage();

  pdf.FinishDocument();

  // Check data size.
  const uint32_t size = pdf.GetDataSize();
  EXPECT_GT(size, 0U);

  // Get resulting data.
  std::vector<char> buffer(size, 0);
  pdf.GetData(&buffer.front(), size);

  // Test browser-side constructor.
  PdfMetafileCg pdf2;
  // TODO(thestig): Make `buffer` uint8_t and avoid the base::as_bytes() call.
  EXPECT_TRUE(pdf2.InitFromData(base::as_bytes(base::make_span(buffer))));

  // Get the first 4 characters from pdf2.
  std::vector<char> buffer2(4, 0);
  pdf2.GetData(&buffer2.front(), 4);

  // Test that the header begins with "%PDF".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(0U, header.find("%PDF", 0));

  // Test that the PDF is correctly reconstructed.
  EXPECT_EQ(2U, pdf2.GetPageCount());
  gfx::Size page_size = pdf2.GetPageBounds(1).size();
  EXPECT_EQ(540, page_size.width());
  EXPECT_EQ(720, page_size.height());
  page_size = pdf2.GetPageBounds(2).size();
  EXPECT_EQ(720, page_size.width());
  EXPECT_EQ(540, page_size.height());
}

TEST(PdfMetafileCgTest, GetPageBounds) {
  // Get test data.
  base::FilePath pdf_file = GetPdfTestData("rectangles_multi_pages.pdf");
  ASSERT_FALSE(pdf_file.empty());
  std::string pdf_data;
  ASSERT_TRUE(base::ReadFileToString(pdf_file, &pdf_data));

  // Initialize and check metafile.
  PdfMetafileCg pdf_cg;
  ASSERT_TRUE(pdf_cg.InitFromData(base::as_bytes(base::make_span(pdf_data))));
  ASSERT_EQ(5u, pdf_cg.GetPageCount());

  // Since the input into GetPageBounds() is a 1-indexed page number, 0 and 6
  // are out of bounds.
  gfx::Rect bounds;
  for (size_t i : {0, 6}) {
    bounds = pdf_cg.GetPageBounds(i);
    EXPECT_EQ(0, bounds.x());
    EXPECT_EQ(0, bounds.y());
    EXPECT_EQ(0, bounds.width());
    EXPECT_EQ(0, bounds.height());
  }

  // Whereas 1-5 are in bounds.
  for (size_t i = 1; i < 6; ++i) {
    bounds = pdf_cg.GetPageBounds(i);
    EXPECT_EQ(0, bounds.x());
    EXPECT_EQ(0, bounds.y());
    EXPECT_EQ(200, bounds.width());
    EXPECT_EQ(250, bounds.height());
  }
}

TEST(PdfMetafileCgTest, RenderPortraitRectangles) {
  constexpr gfx::Rect kPageBounds(200, 300);
  constexpr gfx::Size kDestinationSize(200, 300);
  TestRenderPage("rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_portrait_rectangles_expected.0.png", kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderAutorotatedPortraitRectangles) {
  constexpr gfx::Rect kPageBounds(200, 300);
  constexpr gfx::Size kDestinationSize(300, 200);
  TestRenderPage("rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_autorotated_portrait_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderLargePortraitRectangles) {
  constexpr gfx::Rect kPageBounds(200, 300);
  constexpr gfx::Size kDestinationSize(100, 120);
  TestRenderPage("rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_large_portrait_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderSmallPortraitRectangles) {
  constexpr gfx::Rect kPageBounds(200, 300);
  constexpr gfx::Size kDestinationSize(300, 450);
  TestRenderPage("rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_small_portrait_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderLandscapeRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kDestinationSize(400, 600);
  TestRenderPage("landscape_rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_landscape_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderRotatedRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kLandscapeDestinationSize(600, 400);
  constexpr gfx::Size kPortraitDestinationSize(400, 600);

  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_rotated_rectangles_expected.0.png",
                 kLandscapeDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/2, kPageBounds,
                 "render_rotated_rectangles_expected.1.png",
                 kPortraitDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/3, kPageBounds,
                 "render_rotated_rectangles_expected.2.png",
                 kLandscapeDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/4, kPageBounds,
                 "render_rotated_rectangles_expected.3.png",
                 kPortraitDestinationSize);

  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_autorotated_rotated_rectangles_expected.0.png",
                 kPortraitDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/2, kPageBounds,
                 "render_autorotated_rotated_rectangles_expected.1.png",
                 kLandscapeDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/3, kPageBounds,
                 "render_autorotated_rotated_rectangles_expected.2.png",
                 kPortraitDestinationSize);
  TestRenderPage("rotated_rectangles.pdf", /*page_number=*/4, kPageBounds,
                 "render_autorotated_rotated_rectangles_expected.3.png",
                 kLandscapeDestinationSize);
}

TEST(PdfMetafileCgTest, RenderLargeLandscapeRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kDestinationSize(200, 300);
  TestRenderPage("landscape_rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_large_landscape_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderSmallLandscapeRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kDestinationSize(600, 900);
  TestRenderPage("landscape_rectangles.pdf", /*page_number=*/1, kPageBounds,
                 "render_small_landscape_rectangles_expected.0.png",
                 kDestinationSize);
}

TEST(PdfMetafileCgTest, RenderScaledLargeLandscapeRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kDestinationSize(300, 450);
  TestRenderPageWithTransformParams(
      "landscape_rectangles.pdf", /*page_number=*/1, kPageBounds,
      "render_scaled_large_landscape_rectangles_expected.0.png",
      kDestinationSize,
      /*autorotate=*/true, /*fit_to_page=*/true);
}

TEST(PdfMetafileCgTest, RenderScaledSmallLandscapeRectangles) {
  constexpr gfx::Rect kPageBounds(800, 500);
  constexpr gfx::Size kDestinationSize(600, 900);
  TestRenderPageWithTransformParams(
      "landscape_rectangles.pdf", /*page_number=*/1, kPageBounds,
      "render_scaled_small_landscape_rectangles_expected.0.png",
      kDestinationSize,
      /*autorotate=*/true, /*fit_to_page=*/true);
}

}  // namespace printing
