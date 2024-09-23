// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "pdf/flatten_pdf_result.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "printing/nup_parameters.h"
#include "printing/page_setup.h"
#include "printing/units.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/pdfium/public/fpdf_flatten.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_transformpage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using printing::ConvertUnit;
using printing::ConvertUnitFloat;
using printing::kPixelsPerInch;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

// UI should have done parameter sanity check, when execution
// reaches here, `pages_per_sheet` should be a positive integer.
bool ShouldDoNup(int pages_per_sheet) {
  return pages_per_sheet > 1;
}

// Returns the valid, positive page count, or std::nullopt on failure.
std::optional<uint32_t> GetDocumentPageCount(FPDF_DOCUMENT doc) {
  const int32_t page_count = FPDF_GetPageCount(doc);
  if (page_count <= 0) {
    return std::nullopt;
  }
  return page_count;
}

// Set the destination page size and content area in points based on source
// page rotation and orientation.
//
// `rotated` True if source page is rotated 90 degree or 270 degree.
// `is_src_page_landscape` is true if the source page orientation is landscape.
// `page_size` has the actual destination page size in points.
// `content_rect` has the actual destination page printable area values in
// points.
void SetPageSizeAndContentRect(bool rotated,
                               bool is_src_page_landscape,
                               gfx::Size* page_size,
                               gfx::Rect* content_rect) {
  bool is_dst_page_landscape = page_size->width() > page_size->height();
  bool page_orientation_mismatched =
      is_src_page_landscape != is_dst_page_landscape;
  bool rotate_dst_page = rotated ^ page_orientation_mismatched;
  if (rotate_dst_page) {
    page_size->SetSize(page_size->height(), page_size->width());
    content_rect->SetRect(content_rect->y(), content_rect->x(),
                          content_rect->height(), content_rect->width());
  }
}

// Transform `page` contents to fit in the selected printer paper size.
void TransformPDFPageForPrinting(
    FPDF_PAGE page,
    float scale_factor,
    printing::mojom::PrintScalingOption scaling_option,
    const gfx::Size& paper_size,
    const gfx::Rect& printable_area) {
  // Get the source page width and height in points.
  gfx::SizeF src_page_size(FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page));
  const int src_page_rotation = FPDFPage_GetRotation(page);

  gfx::Size page_size = paper_size;
  gfx::Rect content_rect = printable_area;
  const bool rotated = (src_page_rotation % 2 == 1);
  SetPageSizeAndContentRect(rotated,
                            src_page_size.width() > src_page_size.height(),
                            &page_size, &content_rect);

  // Compute the screen page width and height in points.
  const int actual_page_width =
      rotated ? page_size.height() : page_size.width();
  const int actual_page_height =
      rotated ? page_size.width() : page_size.height();

  gfx::Rect gfx_printed_rect;
  bool fitted_scaling;
  switch (scaling_option) {
    case printing::mojom::PrintScalingOption::kFitToPrintableArea:
      gfx_printed_rect = gfx::Rect(content_rect.x(), content_rect.y(),
                                   content_rect.width(), content_rect.height());
      fitted_scaling = true;
      break;
    case printing::mojom::PrintScalingOption::kFitToPaper:
      gfx_printed_rect = gfx::Rect(page_size.width(), page_size.height());
      fitted_scaling = true;
      break;
    default:
      fitted_scaling = false;
      break;
  }

  if (fitted_scaling) {
    scale_factor =
        CalculateScaleFactor(gfx_printed_rect, src_page_size, rotated);
  }

  // Calculate positions for the clip box.
  PdfRectangle media_box;
  PdfRectangle crop_box;
  bool has_media_box =
      !!FPDFPage_GetMediaBox(page, &media_box.left, &media_box.bottom,
                             &media_box.right, &media_box.top);
  bool has_crop_box = !!FPDFPage_GetCropBox(
      page, &crop_box.left, &crop_box.bottom, &crop_box.right, &crop_box.top);
  CalculateMediaBoxAndCropBox(rotated, has_media_box, has_crop_box, &media_box,
                              &crop_box);
  PdfRectangle source_clip_box = CalculateClipBoxBoundary(media_box, crop_box);
  ScalePdfRectangle(scale_factor, &source_clip_box);

  // Calculate the translation offset values.
  gfx::PointF offset =
      fitted_scaling
          ? CalculateScaledClipBoxOffset(gfx_printed_rect, source_clip_box)
          : CalculateNonScaledClipBoxOffset(
                src_page_rotation, actual_page_width, actual_page_height,
                source_clip_box);

  // Reset the media box and crop box. When the page has crop box and media box,
  // the plugin will display the crop box contents and not the entire media box.
  // If the pages have different crop box values, the plugin will display a
  // document of multiple page sizes. To give better user experience, we
  // decided to have same crop box and media box values. Hence, the user will
  // see a list of uniform pages.
  FPDFPage_SetMediaBox(page, 0, 0, page_size.width(), page_size.height());
  FPDFPage_SetCropBox(page, 0, 0, page_size.width(), page_size.height());

  // Transformation is not required, return. Do this check only after updating
  // the media box and crop box. For more detailed information, please refer to
  // the comment block right before FPDF_SetMediaBox and FPDF_GetMediaBox calls.
  if (scale_factor == 1.0f && offset.IsOrigin())
    return;

  // All the positions have been calculated, now manipulate the PDF.
  const FS_MATRIX matrix = {scale_factor, 0.0f,       0.0f,
                            scale_factor, offset.x(), offset.y()};
  const FS_RECTF cliprect = {
      source_clip_box.left + offset.x(), source_clip_box.top + offset.y(),
      source_clip_box.right + offset.x(), source_clip_box.bottom + offset.y()};
  FPDFPage_TransFormWithClip(page, &matrix, &cliprect);
  FPDFPage_TransformAnnots(page, scale_factor, 0, 0, scale_factor, offset.x(),
                           offset.y());
}

void FitContentsToPrintableAreaIfRequired(
    FPDF_DOCUMENT doc,
    float scale_factor,
    printing::mojom::PrintScalingOption scaling_option,
    const gfx::Size& paper_size,
    const gfx::Rect& printable_area) {
  // Check to see if we need to fit pdf contents to printer paper size.
  if (scaling_option == printing::mojom::PrintScalingOption::kSourceSize)
    return;

  int num_pages = FPDF_GetPageCount(doc);
  // In-place transformation is more efficient than creating a new
  // transformed document from the source document. Therefore, transform
  // every page to fit the contents in the selected printer paper.
  for (int i = 0; i < num_pages; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(doc, i));
    TransformPDFPageForPrinting(page.get(), scale_factor, scaling_option,
                                paper_size, printable_area);
  }
}

// Takes the same parameters as PDFiumPrint::CreateNupPdf().
// On success, returns the N-up version of `doc`. On failure, returns nullptr.
ScopedFPDFDocument CreateNupPdfDocument(ScopedFPDFDocument doc,
                                        size_t pages_per_sheet,
                                        const gfx::Size& page_size,
                                        const gfx::Rect& printable_area) {
  DCHECK(doc);
  DCHECK(ShouldDoNup(pages_per_sheet));

  int page_size_width = page_size.width();
  int page_size_height = page_size.height();

  printing::NupParameters nup_params(
      pages_per_sheet, PDFiumPrint::IsSourcePdfLandscape(doc.get()));
  bool paper_is_landscape = page_size_width > page_size_height;
  if (nup_params.landscape() != paper_is_landscape)
    std::swap(page_size_width, page_size_height);

  ScopedFPDFDocument nup_doc(FPDF_ImportNPagesToOne(
      doc.get(), page_size_width, page_size_height,
      nup_params.num_pages_on_x_axis(), nup_params.num_pages_on_y_axis()));
  if (nup_doc) {
    PDFiumPrint::FitContentsToPrintableArea(nup_doc.get(), page_size,
                                            printable_area);
  }
  return nup_doc;
}

std::vector<uint8_t> ConvertDocToBuffer(ScopedFPDFDocument doc) {
  DCHECK(doc);

  PDFiumMemBufferFileWrite output_file_write;
  if (!FPDF_SaveAsCopy(doc.get(), &output_file_write, 0)) {
    return std::vector<uint8_t>();
  }
  return output_file_write.TakeBuffer();
}

int GetBlockForJpeg(void* param,
                    unsigned long pos,
                    unsigned char* buf,
                    unsigned long size) {
  std::vector<uint8_t>* data_vector = static_cast<std::vector<uint8_t>*>(param);
  if (pos + size < pos || pos + size > data_vector->size()) {
    return 0;
  }
  auto data_span = base::make_span(*data_vector).subspan(pos, size);
  memcpy(buf, data_span.data(), data_span.size());
  return 1;
}

// On success returns the number of flattened pages.
// On failure returns std::nullopt.
std::optional<uint32_t> FlattenPrintData(FPDF_DOCUMENT doc) {
  DCHECK(doc);

  std::optional<uint32_t> page_count = GetDocumentPageCount(doc);
  if (!page_count) {
    return std::nullopt;
  }
  for (uint32_t i = 0; i < *page_count; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(doc, i));
    DCHECK(page);
    if (FPDFPage_Flatten(page.get(), FLAT_PRINT) == FLATTEN_FAIL) {
      return std::nullopt;
    }
  }
  return *page_count;
}

gfx::RectF CSSPixelsToPoints(const gfx::RectF& rect) {
  return gfx::RectF(
      ConvertUnitFloat(rect.x(), kPixelsPerInch, kPointsPerInch),
      ConvertUnitFloat(rect.y(), kPixelsPerInch, kPointsPerInch),
      ConvertUnitFloat(rect.width(), kPixelsPerInch, kPointsPerInch),
      ConvertUnitFloat(rect.height(), kPixelsPerInch, kPointsPerInch));
}

gfx::SizeF CSSPixelsToPoints(const gfx::SizeF& size) {
  return gfx::SizeF(
      ConvertUnitFloat(size.width(), kPixelsPerInch, kPointsPerInch),
      ConvertUnitFloat(size.height(), kPixelsPerInch, kPointsPerInch));
}

}  // namespace

PDFiumPrint::PDFiumPrint(PDFiumEngine* engine) : engine_(engine) {}

PDFiumPrint::~PDFiumPrint() = default;

#if BUILDFLAG(IS_CHROMEOS)
// static
std::optional<FlattenPdfResult> PDFiumPrint::CreateFlattenedPdf(
    ScopedFPDFDocument doc) {
  std::optional<uint32_t> pages_flattened = FlattenPrintData(doc.get());
  if (!pages_flattened) {
    return std::nullopt;
  }
  std::vector<uint8_t> buffer = ConvertDocToBuffer(std::move(doc));
  if (buffer.empty()) {
    return std::nullopt;
  }
  return FlattenPdfResult(std::move(buffer), *pages_flattened);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
std::vector<uint8_t> PDFiumPrint::CreateNupPdf(
    ScopedFPDFDocument doc,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  ScopedFPDFDocument nup_doc = CreateNupPdfDocument(
      std::move(doc), pages_per_sheet, page_size, printable_area);
  return nup_doc ? ConvertDocToBuffer(std::move(nup_doc))
                 : std::vector<uint8_t>();
}

// static
bool PDFiumPrint::IsSourcePdfLandscape(FPDF_DOCUMENT doc) {
  DCHECK(doc);

  ScopedFPDFPage pdf_page(FPDF_LoadPage(doc, 0));
  DCHECK(pdf_page);

  bool is_source_landscape =
      FPDF_GetPageWidthF(pdf_page.get()) > FPDF_GetPageHeightF(pdf_page.get());
  return is_source_landscape;
}

// static
void PDFiumPrint::FitContentsToPrintableArea(FPDF_DOCUMENT doc,
                                             const gfx::Size& page_size,
                                             const gfx::Rect& printable_area) {
  FitContentsToPrintableAreaIfRequired(
      doc, /*scale_factor=*/1.0f,
      printing::mojom::PrintScalingOption::kFitToPrintableArea, page_size,
      printable_area);
}

std::vector<uint8_t> PDFiumPrint::PrintPagesAsPdf(
    const std::vector<int>& page_indices,
    const blink::WebPrintParams& print_params) {
  ScopedFPDFDocument output_doc = CreatePrintPdf(page_indices, print_params);
  if (print_params.rasterize_pdf) {
    output_doc =
        CreateRasterPdf(std::move(output_doc), print_params.printer_dpi);
  }
  return GetDocumentPageCount(output_doc.get())
             ? ConvertDocToBuffer(std::move(output_doc))
             : std::vector<uint8_t>();
}

ScopedFPDFDocument PDFiumPrint::CreatePrintPdf(
    const std::vector<int>& page_indices,
    const blink::WebPrintParams& print_params) {
  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  DCHECK(output_doc);
  FPDF_CopyViewerPreferences(output_doc.get(), engine_->doc());

  if (!FPDF_ImportPagesByIndex(output_doc.get(), engine_->doc(),
                               page_indices.data(), page_indices.size(),
                               /*index=*/0)) {
    return nullptr;
  }

  gfx::Size int_paper_size = ToFlooredSize(
      CSSPixelsToPoints(print_params.default_page_description.size));
  gfx::Rect int_printable_area = ToEnclosedRect(
      CSSPixelsToPoints(print_params.printable_area_in_css_pixels));

  FitContentsToPrintableAreaIfRequired(
      output_doc.get(), print_params.scale_factor,
      print_params.print_scaling_option, int_paper_size, int_printable_area);
  if (!FlattenPrintData(output_doc.get()))
    return nullptr;

  uint32_t pages_per_sheet = print_params.pages_per_sheet;
  if (!ShouldDoNup(pages_per_sheet))
    return output_doc;

  gfx::Rect symmetrical_printable_area =
      printing::PageSetup::GetSymmetricalPrintableArea(int_paper_size,
                                                       int_printable_area);
  if (symmetrical_printable_area.IsEmpty())
    return nullptr;
  return CreateNupPdfDocument(std::move(output_doc), pages_per_sheet,
                              int_paper_size, symmetrical_printable_area);
}

ScopedFPDFDocument PDFiumPrint::CreateRasterPdf(ScopedFPDFDocument doc,
                                                int dpi) {
  std::optional<uint32_t> page_count = GetDocumentPageCount(doc.get());
  if (!page_count) {
    return nullptr;
  }

  ScopedFPDFDocument rasterized_doc(FPDF_CreateNewDocument());
  DCHECK(rasterized_doc);
  FPDF_CopyViewerPreferences(rasterized_doc.get(), doc.get());

  for (uint32_t i = 0; i < *page_count; ++i) {
    ScopedFPDFPage pdf_page(FPDF_LoadPage(doc.get(), i));
    if (!pdf_page)
      return nullptr;

    ScopedFPDFDocument temp_doc =
        CreateSinglePageRasterPdf(pdf_page.get(), dpi);
    if (!temp_doc)
      return nullptr;

    if (!FPDF_ImportPages(rasterized_doc.get(), temp_doc.get(), "1", i))
      return nullptr;
  }

  return rasterized_doc;
}

ScopedFPDFDocument PDFiumPrint::CreateSinglePageRasterPdf(
    FPDF_PAGE page_to_print,
    int dpi) {
  ScopedFPDFDocument temp_doc(FPDF_CreateNewDocument());
  DCHECK(temp_doc);

  float source_page_width = FPDF_GetPageWidthF(page_to_print);
  float source_page_height = FPDF_GetPageHeightF(page_to_print);

  // For computing size in pixels, use square pixels since the source PDF page
  // has square pixels.
  int width_in_pixels = ConvertUnit(source_page_width, kPointsPerInch, dpi);
  int height_in_pixels = ConvertUnit(source_page_height, kPointsPerInch, dpi);

  gfx::Size bitmap_size(width_in_pixels, height_in_pixels);
  ScopedFPDFBitmap bitmap(FPDFBitmap_Create(
      bitmap_size.width(), bitmap_size.height(), /*alpha=*/false));

  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, bitmap_size.width(),
                      bitmap_size.height(), 0xFFFFFFFF);

  FPDF_RenderPageBitmap(bitmap.get(), page_to_print, 0, 0, bitmap_size.width(),
                        bitmap_size.height(),
                        ToPDFiumRotation(PageOrientation::kOriginal),
                        FPDF_PRINTING);

  float ratio_x = ConvertUnitFloat(bitmap_size.width(), dpi, kPointsPerInch);
  float ratio_y = ConvertUnitFloat(bitmap_size.height(), dpi, kPointsPerInch);

  // Add the bitmap to an image object and add the image object to the output
  // page.
  ScopedFPDFPageObject temp_img(FPDFPageObj_NewImageObj(temp_doc.get()));

  // Use quality = 40 as this does not significantly degrade the printed
  // document relative to a normal bitmap and provides better compression than
  // a higher quality setting.
  constexpr int kQuality = 40;
  SkImageInfo info = SkImageInfo::Make(
      FPDFBitmap_GetWidth(bitmap.get()), FPDFBitmap_GetHeight(bitmap.get()),
      kBGRA_8888_SkColorType, kOpaque_SkAlphaType);
  SkPixmap src(info, FPDFBitmap_GetBuffer(bitmap.get()),
               FPDFBitmap_GetStride(bitmap.get()));
  std::vector<uint8_t> compressed_bitmap_data;
  bool encoded = gfx::JPEGCodec::Encode(src, kQuality, &compressed_bitmap_data);

  ScopedFPDFPage temp_page_holder(
      FPDFPage_New(temp_doc.get(), 0, source_page_width, source_page_height));
  FPDF_PAGE temp_page = temp_page_holder.get();
  if (encoded) {
    FPDF_FILEACCESS file_access = {};
    file_access.m_FileLen =
        static_cast<unsigned long>(compressed_bitmap_data.size());
    file_access.m_GetBlock = &GetBlockForJpeg;
    file_access.m_Param = &compressed_bitmap_data;

    FPDFImageObj_LoadJpegFileInline(&temp_page, 1, temp_img.get(),
                                    &file_access);
  } else {
    FPDFImageObj_SetBitmap(&temp_page, 1, temp_img.get(), bitmap.get());
  }

  FPDFImageObj_SetMatrix(temp_img.get(), ratio_x, 0, 0, ratio_y, 0, 0);
  FPDFPage_InsertObject(temp_page, temp_img.release());
  FPDFPage_GenerateContent(temp_page);

  return temp_doc;
}

}  // namespace chrome_pdf
