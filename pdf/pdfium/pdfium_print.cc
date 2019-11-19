// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_read.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "printing/nup_parameters.h"
#include "printing/page_setup.h"
#include "printing/units.h"
#include "third_party/pdfium/public/fpdf_flatten.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_transformpage.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

// UI should have done parameter sanity check, when execution
// reaches here, |pages_per_sheet| should be a positive integer.
bool ShouldDoNup(int pages_per_sheet) {
  return pages_per_sheet > 1;
}

// Returns the valid, positive page count, or 0 on failure.
int GetDocumentPageCount(FPDF_DOCUMENT doc) {
  return std::max(FPDF_GetPageCount(doc), 0);
}

// Set the destination page size and content area in points based on source
// page rotation and orientation.
//
// |rotated| True if source page is rotated 90 degree or 270 degree.
// |is_src_page_landscape| is true if the source page orientation is landscape.
// |page_size| has the actual destination page size in points.
// |content_rect| has the actual destination page printable area values in
// points.
void SetPageSizeAndContentRect(bool rotated,
                               bool is_src_page_landscape,
                               pp::Size* page_size,
                               pp::Rect* content_rect) {
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

// Transform |page| contents to fit in the selected printer paper size.
void TransformPDFPageForPrinting(FPDF_PAGE page,
                                 double scale_factor,
                                 const PP_PrintSettings_Dev& print_settings) {
  // Get the source page width and height in points.
  const double src_page_width = FPDF_GetPageWidth(page);
  const double src_page_height = FPDF_GetPageHeight(page);
  const int src_page_rotation = FPDFPage_GetRotation(page);

  pp::Size page_size(print_settings.paper_size);
  pp::Rect content_rect(print_settings.printable_area);
  const bool rotated = (src_page_rotation % 2 == 1);
  SetPageSizeAndContentRect(rotated, src_page_width > src_page_height,
                            &page_size, &content_rect);

  // Compute the screen page width and height in points.
  const int actual_page_width =
      rotated ? page_size.height() : page_size.width();
  const int actual_page_height =
      rotated ? page_size.width() : page_size.height();

  gfx::Rect gfx_printed_rect;
  bool fitted_scaling;
  switch (print_settings.print_scaling_option) {
    case PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA:
      gfx_printed_rect = gfx::Rect(content_rect.x(), content_rect.y(),
                                   content_rect.width(), content_rect.height());
      fitted_scaling = true;
      break;
    case PP_PRINTSCALINGOPTION_FIT_TO_PAPER:
      gfx_printed_rect = gfx::Rect(page_size.width(), page_size.height());
      fitted_scaling = true;
      break;
    default:
      fitted_scaling = false;
      break;
  }

  if (fitted_scaling) {
    scale_factor = CalculateScaleFactor(gfx_printed_rect, src_page_width,
                                        src_page_height, rotated);
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
  double offset_x = 0;
  double offset_y = 0;

  if (fitted_scaling) {
    CalculateScaledClipBoxOffset(gfx_printed_rect, source_clip_box, &offset_x,
                                 &offset_y);
  } else {
    CalculateNonScaledClipBoxOffset(src_page_rotation, actual_page_width,
                                    actual_page_height, source_clip_box,
                                    &offset_x, &offset_y);
  }

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
  if (scale_factor == 1.0 && offset_x == 0 && offset_y == 0)
    return;

  // All the positions have been calculated, now manipulate the PDF.
  FS_MATRIX matrix = {static_cast<float>(scale_factor),
                      0,
                      0,
                      static_cast<float>(scale_factor),
                      static_cast<float>(offset_x),
                      static_cast<float>(offset_y)};
  FS_RECTF cliprect = {static_cast<float>(source_clip_box.left + offset_x),
                       static_cast<float>(source_clip_box.top + offset_y),
                       static_cast<float>(source_clip_box.right + offset_x),
                       static_cast<float>(source_clip_box.bottom + offset_y)};
  FPDFPage_TransFormWithClip(page, &matrix, &cliprect);
  FPDFPage_TransformAnnots(page, scale_factor, 0, 0, scale_factor, offset_x,
                           offset_y);
}

void FitContentsToPrintableAreaIfRequired(
    FPDF_DOCUMENT doc,
    double scale_factor,
    const PP_PrintSettings_Dev& print_settings) {
  // Check to see if we need to fit pdf contents to printer paper size.
  if (print_settings.print_scaling_option == PP_PRINTSCALINGOPTION_SOURCE_SIZE)
    return;

  int num_pages = FPDF_GetPageCount(doc);
  // In-place transformation is more efficient than creating a new
  // transformed document from the source document. Therefore, transform
  // every page to fit the contents in the selected printer paper.
  for (int i = 0; i < num_pages; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(doc, i));
    TransformPDFPageForPrinting(page.get(), scale_factor, print_settings);
  }
}

// Takes the same parameters as PDFiumPrint::CreateNupPdf().
// On success, returns the N-up version of |doc|. On failure, returns nullptr.
ScopedFPDFDocument CreateNupPdfDocument(ScopedFPDFDocument doc,
                                        size_t pages_per_sheet,
                                        const gfx::Size& page_size,
                                        const gfx::Rect& printable_area) {
  DCHECK(doc);
  DCHECK(ShouldDoNup(pages_per_sheet));

  int page_size_width = page_size.width();
  int page_size_height = page_size.height();

  printing::NupParameters nup_params;
  bool is_landscape = PDFiumPrint::IsSourcePdfLandscape(doc.get());
  nup_params.SetParameters(pages_per_sheet, is_landscape);
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

  std::vector<uint8_t> buffer;
  PDFiumMemBufferFileWrite output_file_write;
  if (FPDF_SaveAsCopy(doc.get(), &output_file_write, 0))
    buffer = output_file_write.TakeBuffer();
  return buffer;
}

int GetBlockForJpeg(void* param,
                    unsigned long pos,
                    unsigned char* buf,
                    unsigned long size) {
  std::vector<uint8_t>* data_vector = static_cast<std::vector<uint8_t>*>(param);
  if (pos + size < pos || pos + size > data_vector->size())
    return 0;
  memcpy(buf, data_vector->data() + pos, size);
  return 1;
}

std::string GetPageRangeStringFromRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  DCHECK(page_range_count);

  std::string page_number_str;
  for (uint32_t i = 0; i < page_range_count; ++i) {
    if (!page_number_str.empty())
      page_number_str.push_back(',');
    const PP_PrintPageNumberRange_Dev& range = page_ranges[i];
    page_number_str.append(base::NumberToString(range.first_page_number + 1));
    if (range.first_page_number != range.last_page_number) {
      page_number_str.push_back('-');
      page_number_str.append(base::NumberToString(range.last_page_number + 1));
    }
  }
  return page_number_str;
}

bool FlattenPrintData(FPDF_DOCUMENT doc) {
  DCHECK(doc);

  int page_count = FPDF_GetPageCount(doc);
  for (int i = 0; i < page_count; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(doc, i));
    DCHECK(page);
    if (FPDFPage_Flatten(page.get(), FLAT_PRINT) == FLATTEN_FAIL)
      return false;
  }
  return true;
}

}  // namespace

PDFiumPrint::PDFiumPrint(PDFiumEngine* engine) : engine_(engine) {}

PDFiumPrint::~PDFiumPrint() = default;

#if defined(OS_CHROMEOS)
// static
std::vector<uint8_t> PDFiumPrint::CreateFlattenedPdf(ScopedFPDFDocument doc) {
  if (!FlattenPrintData(doc.get()))
    return std::vector<uint8_t>();
  return ConvertDocToBuffer(std::move(doc));
}
#endif  // defined(OS_CHROMEOS)

// static
std::vector<uint32_t> PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  DCHECK(page_range_count);

  std::vector<uint32_t> page_numbers;
  for (uint32_t i = 0; i < page_range_count; ++i) {
    for (uint32_t page_number = page_ranges[i].first_page_number;
         page_number <= page_ranges[i].last_page_number; ++page_number) {
      page_numbers.push_back(page_number);
    }
  }
  return page_numbers;
}

// static
std::vector<uint8_t> PDFiumPrint::CreateNupPdf(
    ScopedFPDFDocument doc,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  ScopedFPDFDocument nup_doc = CreateNupPdfDocument(
      std::move(doc), pages_per_sheet, page_size, printable_area);
  if (!nup_doc)
    return std::vector<uint8_t>();
  return ConvertDocToBuffer(std::move(nup_doc));
}

// static
bool PDFiumPrint::IsSourcePdfLandscape(FPDF_DOCUMENT doc) {
  DCHECK(doc);

  ScopedFPDFPage pdf_page(FPDF_LoadPage(doc, 0));
  DCHECK(pdf_page);

  bool is_source_landscape =
      FPDF_GetPageWidth(pdf_page.get()) > FPDF_GetPageHeight(pdf_page.get());
  return is_source_landscape;
}

// static
void PDFiumPrint::FitContentsToPrintableArea(FPDF_DOCUMENT doc,
                                             const gfx::Size& page_size,
                                             const gfx::Rect& printable_area) {
  PP_PrintSettings_Dev print_settings;
  print_settings.paper_size = pp::Size(page_size.width(), page_size.height());
  print_settings.printable_area =
      pp::Rect(printable_area.x(), printable_area.y(), printable_area.width(),
               printable_area.height());
  print_settings.print_scaling_option =
      PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA;
  FitContentsToPrintableAreaIfRequired(doc, 1.0, print_settings);
}

std::vector<uint8_t> PDFiumPrint::PrintPagesAsPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings,
    bool raster) {
  std::vector<uint8_t> buffer;
  ScopedFPDFDocument output_doc = CreatePrintPdf(
      page_ranges, page_range_count, print_settings, pdf_print_settings);
  if (raster)
    output_doc = CreateRasterPdf(std::move(output_doc), print_settings);
  if (GetDocumentPageCount(output_doc.get()))
    buffer = ConvertDocToBuffer(std::move(output_doc));
  return buffer;
}

ScopedFPDFDocument PDFiumPrint::CreatePrintPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  DCHECK(output_doc);
  FPDF_CopyViewerPreferences(output_doc.get(), engine_->doc());

  std::string page_number_str =
      GetPageRangeStringFromRange(page_ranges, page_range_count);
  if (!FPDF_ImportPages(output_doc.get(), engine_->doc(),
                        page_number_str.c_str(), 0)) {
    return nullptr;
  }

  double scale_factor = pdf_print_settings.scale_factor / 100.0;
  FitContentsToPrintableAreaIfRequired(output_doc.get(), scale_factor,
                                       print_settings);
  if (!FlattenPrintData(output_doc.get()))
    return nullptr;

  uint32_t pages_per_sheet = pdf_print_settings.pages_per_sheet;
  if (!ShouldDoNup(pages_per_sheet))
    return output_doc;

  gfx::Size page_size(print_settings.paper_size.width,
                      print_settings.paper_size.height);
  gfx::Rect printable_area(print_settings.printable_area.point.x,
                           print_settings.printable_area.point.y,
                           print_settings.printable_area.size.width,
                           print_settings.printable_area.size.height);
  gfx::Rect symmetrical_printable_area =
      printing::PageSetup::GetSymmetricalPrintableArea(page_size,
                                                       printable_area);
  if (symmetrical_printable_area.IsEmpty())
    return nullptr;
  return CreateNupPdfDocument(std::move(output_doc), pages_per_sheet, page_size,
                              symmetrical_printable_area);
}

ScopedFPDFDocument PDFiumPrint::CreateRasterPdf(
    ScopedFPDFDocument doc,
    const PP_PrintSettings_Dev& print_settings) {
  int page_count = GetDocumentPageCount(doc.get());
  if (page_count == 0)
    return nullptr;

  ScopedFPDFDocument rasterized_doc(FPDF_CreateNewDocument());
  DCHECK(rasterized_doc);
  FPDF_CopyViewerPreferences(rasterized_doc.get(), doc.get());

  for (int i = 0; i < page_count; ++i) {
    ScopedFPDFPage pdf_page(FPDF_LoadPage(doc.get(), i));
    if (!pdf_page)
      return nullptr;

    ScopedFPDFDocument temp_doc =
        CreateSinglePageRasterPdf(pdf_page.get(), print_settings);
    if (!temp_doc)
      return nullptr;

    if (!FPDF_ImportPages(rasterized_doc.get(), temp_doc.get(), "1", i))
      return nullptr;
  }

  return rasterized_doc;
}

ScopedFPDFDocument PDFiumPrint::CreateSinglePageRasterPdf(
    FPDF_PAGE page_to_print,
    const PP_PrintSettings_Dev& print_settings) {
  ScopedFPDFDocument temp_doc(FPDF_CreateNewDocument());
  DCHECK(temp_doc);

  double source_page_width = FPDF_GetPageWidth(page_to_print);
  double source_page_height = FPDF_GetPageHeight(page_to_print);

  // For computing size in pixels, use a square dpi since the source PDF page
  // has square DPI.
  int width_in_pixels =
      ConvertUnit(source_page_width, kPointsPerInch, print_settings.dpi);
  int height_in_pixels =
      ConvertUnit(source_page_height, kPointsPerInch, print_settings.dpi);

  pp::Size bitmap_size(width_in_pixels, height_in_pixels);
  ScopedFPDFBitmap bitmap(FPDFBitmap_Create(
      bitmap_size.width(), bitmap_size.height(), /*alpha=*/false));

  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, bitmap_size.width(),
                      bitmap_size.height(), 0xFFFFFFFF);

  FPDF_RenderPageBitmap(bitmap.get(), page_to_print, 0, 0, bitmap_size.width(),
                        bitmap_size.height(), print_settings.orientation,
                        FPDF_PRINTING);

  unsigned char* bitmap_data =
      static_cast<unsigned char*>(FPDFBitmap_GetBuffer(bitmap.get()));
  double ratio_x = ConvertUnitDouble(bitmap_size.width(), print_settings.dpi,
                                     kPointsPerInch);
  double ratio_y = ConvertUnitDouble(bitmap_size.height(), print_settings.dpi,
                                     kPointsPerInch);

  // Add the bitmap to an image object and add the image object to the output
  // page.
  FPDF_PAGEOBJECT temp_img = FPDFPageObj_NewImageObj(temp_doc.get());

  bool encoded = false;
  std::vector<uint8_t> compressed_bitmap_data;
  if (!(print_settings.format & PP_PRINTOUTPUTFORMAT_PDF)) {
    // Use quality = 40 as this does not significantly degrade the printed
    // document relative to a normal bitmap and provides better compression than
    // a higher quality setting.
    constexpr int kQuality = 40;
    SkImageInfo info = SkImageInfo::Make(
        FPDFBitmap_GetWidth(bitmap.get()), FPDFBitmap_GetHeight(bitmap.get()),
        kBGRA_8888_SkColorType, kOpaque_SkAlphaType);
    SkPixmap src(info, bitmap_data, FPDFBitmap_GetStride(bitmap.get()));
    encoded = gfx::JPEGCodec::Encode(src, kQuality, &compressed_bitmap_data);
  }

  {
    ScopedFPDFPage temp_page_holder(
        FPDFPage_New(temp_doc.get(), 0, source_page_width, source_page_height));
    FPDF_PAGE temp_page = temp_page_holder.get();
    if (encoded) {
      FPDF_FILEACCESS file_access = {};
      file_access.m_FileLen =
          static_cast<unsigned long>(compressed_bitmap_data.size());
      file_access.m_GetBlock = &GetBlockForJpeg;
      file_access.m_Param = &compressed_bitmap_data;

      FPDFImageObj_LoadJpegFileInline(&temp_page, 1, temp_img, &file_access);
    } else {
      FPDFImageObj_SetBitmap(&temp_page, 1, temp_img, bitmap.get());
    }

    FPDFImageObj_SetMatrix(temp_img, ratio_x, 0, 0, ratio_y, 0, 0);
    FPDFPage_InsertObject(temp_page, temp_img);
    FPDFPage_GenerateContent(temp_page);
  }

  return temp_doc;
}

}  // namespace chrome_pdf
