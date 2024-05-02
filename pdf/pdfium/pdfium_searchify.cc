// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <algorithm>
#include <numbers>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_ocr.h"
#include "pdf/pdfium/pdfium_searchify_font.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdf_save.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace chrome_pdf {

namespace {

std::vector<uint32_t> Utf8ToCharcodes(const std::string& string) {
  std::u16string utf16_str = base::UTF8ToUTF16(string);
  std::vector<uint32_t> charcodes;
  charcodes.reserve(utf16_str.size());
  for (auto c : utf16_str) {
    charcodes.push_back(c);
  }
  return charcodes;
}

struct BoundingBoxOrigin {
  double x;
  double y;
  double theta;
};

// The coordinate systems between OCR and PDF are different. OCR's origin is at
// top-left, so we need to convert them to PDF's bottom-left.
BoundingBoxOrigin ConvertToPdfOrigin(int x,
                                     int y,
                                     int width,
                                     int height,
                                     double angle,
                                     double coordinate_system_height) {
  double theta = angle * std::numbers::pi / 180;
  return {.x = x - (sin(theta) * height),
          .y = coordinate_system_height - (y + cos(theta) * height),
          .theta = -theta};
}

// Project the text object's origin to the baseline's origin.
BoundingBoxOrigin ProjectToBaseline(const BoundingBoxOrigin& origin,
                                    const BoundingBoxOrigin& baseline_origin) {
  // The length between `origin` and `baseline_origin`.
  double length = (origin.x - baseline_origin.x) * cos(baseline_origin.theta) +
                  (origin.y - baseline_origin.y) * sin(baseline_origin.theta);
  return {.x = baseline_origin.x + length * cos(baseline_origin.theta),
          .y = baseline_origin.y + length * sin(baseline_origin.theta),
          .theta = baseline_origin.theta};
}

void AddTextOnImage(FPDF_DOCUMENT document,
                    FPDF_PAGE page,
                    FPDF_FONT font,
                    FPDF_PAGEOBJECT image,
                    screen_ai::mojom::VisualAnnotationPtr annotation) {
  FS_QUADPOINTSF quadpoints;
  if (!FPDFPageObj_GetRotatedBounds(image, &quadpoints)) {
    DLOG(ERROR) << "Failed to get image rendered dimensions";
    return;
  }
  double image_rendered_width = sqrt(pow(quadpoints.x1 - quadpoints.x2, 2) +
                                     pow(quadpoints.y1 - quadpoints.y2, 2));
  double image_rendered_height = sqrt(pow(quadpoints.x2 - quadpoints.x3, 2) +
                                      pow(quadpoints.y2 - quadpoints.y3, 2));
  unsigned int image_pixel_width;
  unsigned int image_pixel_height;
  if (!FPDFImageObj_GetImagePixelSize(image, &image_pixel_width,
                                      &image_pixel_height)) {
    DLOG(ERROR) << "Failed to get image dimensions";
    return;
  }
  FS_MATRIX image_matrix;
  if (!FPDFPageObj_GetMatrix(image, &image_matrix)) {
    DLOG(ERROR) << "Failed to get image matrix";
    return;
  }

  for (const auto& line : annotation->lines) {
    BoundingBoxOrigin baseline_origin = ConvertToPdfOrigin(
        line->baseline_box.x(), line->baseline_box.y(),
        line->baseline_box.width(), line->baseline_box.height(),
        line->baseline_box_angle, image_rendered_height);

    for (const auto& word : line->words) {
      double width = word->bounding_box.width();
      double height = word->bounding_box.height();

      if (width == 0 || height == 0) {
        continue;
      }

      ScopedFPDFPageObject text(
          FPDFPageObj_CreateTextObj(document, font, height));
      CHECK(text);

      std::string word_string = word->word;
      // TODO(crbug.com/41487613): A more accurate width would be the distance
      // from current word's origin to next word's origin.
      if (word->has_space_after) {
        word_string.push_back(' ');
      }

      if (word_string.empty()) {
        DLOG(ERROR) << "Got empty word";
        continue;
      }

      std::vector<uint32_t> charcodes = Utf8ToCharcodes(word_string);
      if (!FPDFText_SetCharcodes(text.get(), charcodes.data(),
                                 charcodes.size())) {
        DLOG(ERROR) << "Failed to set charcodes";
        continue;
      }

      // Make text invisible
      if (!FPDFTextObj_SetTextRenderMode(text.get(),
                                         FPDF_TEXTRENDERMODE_INVISIBLE)) {
        DLOG(ERROR) << "Failed to make text invisible";
        continue;
      }

      float left;
      float bottom;
      float right;
      float top;
      if (!FPDFPageObj_GetBounds(text.get(), &left, &bottom, &right, &top)) {
        DLOG(ERROR) << "Failed to get the bounding box of original text object";
        continue;
      }
      double original_text_object_width = right - left;
      double original_text_object_height = top - bottom;
      CHECK_GT(original_text_object_width, 0);
      CHECK_GT(original_text_object_height, 0);
      double width_scale = width / original_text_object_width;
      double height_scale = height / original_text_object_height;
      FPDFPageObj_Transform(text.get(), width_scale, 0, 0, height_scale, 0, 0);

      // Move text object to the corresponding text position on the full image.
      BoundingBoxOrigin origin = ConvertToPdfOrigin(
          word->bounding_box.x(), word->bounding_box.y(), width, height,
          word->bounding_box_angle, image_rendered_height);
      origin = ProjectToBaseline(origin, baseline_origin);
      double a = cos(origin.theta);
      double b = sin(origin.theta);
      double c = -sin(origin.theta);
      double d = cos(origin.theta);
      double e = origin.x;
      double f = origin.y;
      if (word->direction ==
          screen_ai::mojom::Direction::DIRECTION_RIGHT_TO_LEFT) {
        a = -a;
        b = -b;
        e += cos(origin.theta) * width;
        f += sin(origin.theta) * width;
      }
      FPDFPageObj_Transform(text.get(), a, b, c, d, e, f);

      // Scale from full image size to rendered image size on the PDF.
      FPDFPageObj_Transform(text.get(),
                            image_rendered_width / image_pixel_width, 0, 0,
                            image_rendered_height / image_pixel_height, 0, 0);

      // Apply the image's transformation matrix on the PDF page without the
      // scaling matrix.
      FPDFPageObj_Transform(text.get(), image_matrix.a / image_rendered_width,
                            image_matrix.b / image_rendered_width,
                            image_matrix.c / image_rendered_height,
                            image_matrix.d / image_rendered_height,
                            image_matrix.e, image_matrix.f);

      FPDFPage_InsertObject(page, text.release());
    }
  }
}

}  // namespace

std::vector<uint8_t> PDFiumSearchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback) {
  ScopedFPDFDocument document(
      FPDF_LoadMemDocument64(pdf_buffer.data(), pdf_buffer.size(), nullptr));
  if (!document) {
    DLOG(ERROR) << "Failed to load document";
    return {};
  }
  int page_count = FPDF_GetPageCount(document.get());
  if (page_count == 0) {
    DLOG(ERROR) << "Got zero page count";
    return {};
  }
  std::vector<uint8_t> cid_to_gid_map(CreateCidToGidMap());
  ScopedFPDFFont font(FPDFText_LoadCidType2Font(
      document.get(), kPdfTtf, kPdfTtfSize, kToUnicodeCMap,
      cid_to_gid_map.data(), cid_to_gid_map.size()));
  CHECK(font);
  for (int page_index = 0; page_index < page_count; page_index++) {
    ScopedFPDFPage page(FPDF_LoadPage(document.get(), page_index));
    if (!page) {
      DLOG(ERROR) << "Failed to load page";
      continue;
    }
    int object_count = FPDFPage_CountObjects(page.get());
    for (int object_index = 0; object_index < object_count; object_index++) {
      SkBitmap bitmap =
          GetImageForOcr(document.get(), page.get(), object_index);
      // The object is not an image or failed to get the bitmap from the image.
      if (bitmap.empty()) {
        continue;
      }
      FPDF_PAGEOBJECT image = FPDFPage_GetObject(page.get(), object_index);
      if (!image) {
        DLOG(ERROR) << "Failed to get image object";
        continue;
      }
      auto annotation = perform_ocr_callback.Run(bitmap);
      if (!annotation) {
        DLOG(ERROR) << "Failed to get OCR annotation on the image";
        return {};
      }
      AddTextOnImage(document.get(), page.get(), font.get(), image,
                     std::move(annotation));
    }
    if (!FPDFPage_GenerateContent(page.get())) {
      DLOG(ERROR) << "Failed to generate content";
      return {};
    }
  }
  PDFiumMemBufferFileWrite output_file_write;
  if (!FPDF_SaveAsCopy(document.get(), &output_file_write, 0)) {
    DLOG(ERROR) << "Failed to save the document";
    return {};
  }
  return output_file_write.TakeBuffer();
}

}  // namespace chrome_pdf
