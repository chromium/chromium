// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine_exports.h"

#include <algorithm>
#include <utility>

#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_print.h"
#include "printing/nup_parameters.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

int CalculatePosition(FPDF_PAGE page,
                      const PDFiumEngineExports::RenderingSettings& settings,
                      pp::Rect* dest) {
  // settings.bounds is in terms of the max DPI. Convert page sizes to match.
  int dpi = std::max(settings.dpi_x, settings.dpi_y);
  int page_width = static_cast<int>(
      ConvertUnitDouble(FPDF_GetPageWidth(page), kPointsPerInch, dpi));
  int page_height = static_cast<int>(
      ConvertUnitDouble(FPDF_GetPageHeight(page), kPointsPerInch, dpi));

  // Start by assuming that we will draw exactly to the bounds rect
  // specified.
  *dest = settings.bounds;

  int rotate = 0;  // normal orientation.

  // Auto-rotate landscape pages to print correctly.
  if (settings.autorotate &&
      (dest->width() > dest->height()) != (page_width > page_height)) {
    rotate = 3;  // 90 degrees counter-clockwise.
    std::swap(page_width, page_height);
  }

  // See if we need to scale the output
  bool scale_to_bounds = false;
  if (settings.fit_to_bounds &&
      ((page_width > dest->width()) || (page_height > dest->height()))) {
    scale_to_bounds = true;
  } else if (settings.stretch_to_bounds &&
             ((page_width < dest->width()) || (page_height < dest->height()))) {
    scale_to_bounds = true;
  }

  if (scale_to_bounds) {
    // If we need to maintain aspect ratio, calculate the actual width and
    // height.
    if (settings.keep_aspect_ratio) {
      double scale_factor_x = page_width;
      scale_factor_x /= dest->width();
      double scale_factor_y = page_height;
      scale_factor_y /= dest->height();
      if (scale_factor_x > scale_factor_y) {
        dest->set_height(page_height / scale_factor_x);
      } else {
        dest->set_width(page_width / scale_factor_y);
      }
    }
  } else {
    // We are not scaling to bounds. Draw in the actual page size. If the
    // actual page size is larger than the bounds, the output will be
    // clipped.
    dest->set_width(page_width);
    dest->set_height(page_height);
  }

  // Scale the bounds to device units if DPI is rectangular.
  if (settings.dpi_x != settings.dpi_y) {
    dest->set_width(dest->width() * settings.dpi_x / dpi);
    dest->set_height(dest->height() * settings.dpi_y / dpi);
  }

  if (settings.center_in_bounds) {
    pp::Point offset(
        (settings.bounds.width() * settings.dpi_x / dpi - dest->width()) / 2,
        (settings.bounds.height() * settings.dpi_y / dpi - dest->height()) / 2);
    dest->Offset(offset);
  }
  return rotate;
}

ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_buffer) {
  if (!base::IsValueInRangeForNumericType<int>(pdf_buffer.size()))
    return nullptr;
  return ScopedFPDFDocument(
      FPDF_LoadMemDocument(pdf_buffer.data(), pdf_buffer.size(), nullptr));
}

ScopedFPDFDocument CreatePdfDoc(
    std::vector<base::span<const uint8_t>> input_buffers) {
  if (input_buffers.empty())
    return nullptr;

  ScopedFPDFDocument doc(FPDF_CreateNewDocument());
  size_t index = 0;
  for (auto input_buffer : input_buffers) {
    ScopedFPDFDocument single_page_doc = LoadPdfData(input_buffer);
    if (!FPDF_ImportPages(doc.get(), single_page_doc.get(), "1", index++)) {
      return nullptr;
    }
  }

  return doc;
}

bool IsValidPrintableArea(const gfx::Size& page_size,
                          const gfx::Rect& printable_area) {
  return !printable_area.IsEmpty() && printable_area.x() >= 0 &&
         printable_area.y() >= 0 &&
         printable_area.right() <= page_size.width() &&
         printable_area.bottom() <= page_size.height();
}

}  // namespace

PDFEngineExports::RenderingSettings::RenderingSettings(int dpi_x,
                                                       int dpi_y,
                                                       const pp::Rect& bounds,
                                                       bool fit_to_bounds,
                                                       bool stretch_to_bounds,
                                                       bool keep_aspect_ratio,
                                                       bool center_in_bounds,
                                                       bool autorotate,
                                                       bool use_color)
    : dpi_x(dpi_x),
      dpi_y(dpi_y),
      bounds(bounds),
      fit_to_bounds(fit_to_bounds),
      stretch_to_bounds(stretch_to_bounds),
      keep_aspect_ratio(keep_aspect_ratio),
      center_in_bounds(center_in_bounds),
      autorotate(autorotate),
      use_color(use_color) {}

PDFEngineExports::RenderingSettings::RenderingSettings(
    const RenderingSettings& that) = default;

PDFEngineExports* PDFEngineExports::Get() {
  static base::NoDestructor<PDFiumEngineExports> exports;
  return exports.get();
}

PDFiumEngineExports::PDFiumEngineExports() {}

PDFiumEngineExports::~PDFiumEngineExports() {}

#if defined(OS_WIN)
bool PDFiumEngineExports::RenderPDFPageToDC(
    base::span<const uint8_t> pdf_buffer,
    int page_number,
    const RenderingSettings& settings,
    HDC dc) {
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;
  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_number));
  if (!page)
    return false;

  RenderingSettings new_settings = settings;
  // calculate the page size
  if (new_settings.dpi_x == -1)
    new_settings.dpi_x = GetDeviceCaps(dc, LOGPIXELSX);
  if (new_settings.dpi_y == -1)
    new_settings.dpi_y = GetDeviceCaps(dc, LOGPIXELSY);

  pp::Rect dest;
  int rotate = CalculatePosition(page.get(), new_settings, &dest);

  int save_state = SaveDC(dc);
  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  IntersectClipRect(dc, settings.bounds.x(), settings.bounds.y(),
                    settings.bounds.x() + settings.bounds.width(),
                    settings.bounds.y() + settings.bounds.height());

  int flags = FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH;
  if (!settings.use_color)
    flags |= FPDF_GRAYSCALE;

  // A "temporary" hack. Some PDFs seems to render very slowly if
  // FPDF_RenderPage() is directly used on a printer DC. I suspect it is
  // because of the code to talk Postscript directly to the printer if
  // the printer supports this. Need to discuss this with PDFium. For now,
  // render to a bitmap and then blit the bitmap to the DC if we have been
  // supplied a printer DC.
  int device_type = GetDeviceCaps(dc, TECHNOLOGY);
  if (device_type == DT_RASPRINTER || device_type == DT_PLOTTER) {
    ScopedFPDFBitmap bitmap(
        FPDFBitmap_Create(dest.width(), dest.height(), FPDFBitmap_BGRx));
    // Clear the bitmap
    FPDFBitmap_FillRect(bitmap.get(), 0, 0, dest.width(), dest.height(),
                        0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap.get(), page.get(), 0, 0, dest.width(),
                          dest.height(), rotate, flags);
    int stride = FPDFBitmap_GetStride(bitmap.get());
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dest.width();
    bmi.bmiHeader.biHeight = -dest.height();  // top-down image
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = stride * dest.height();
    StretchDIBits(dc, dest.x(), dest.y(), dest.width(), dest.height(), 0, 0,
                  dest.width(), dest.height(),
                  FPDFBitmap_GetBuffer(bitmap.get()), &bmi, DIB_RGB_COLORS,
                  SRCCOPY);
  } else {
    FPDF_RenderPage(dc, page.get(), dest.x(), dest.y(), dest.width(),
                    dest.height(), rotate, flags);
  }
  RestoreDC(dc, save_state);
  return true;
}

void PDFiumEngineExports::SetPDFEnsureTypefaceCharactersAccessible(
    PDFEnsureTypefaceCharactersAccessible func) {
  FPDF_SetTypefaceAccessibleFunc(
      reinterpret_cast<PDFiumEnsureTypefaceCharactersAccessible>(func));
}

void PDFiumEngineExports::SetPDFUseGDIPrinting(bool enable) {
  FPDF_SetPrintTextWithGDI(enable);
}

void PDFiumEngineExports::SetPDFUsePrintMode(int mode) {
  FPDF_SetPrintMode(mode);
}
#endif  // defined(OS_WIN)

bool PDFiumEngineExports::RenderPDFPageToBitmap(
    base::span<const uint8_t> pdf_buffer,
    int page_number,
    const RenderingSettings& settings,
    void* bitmap_buffer) {
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;
  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_number));
  if (!page)
    return false;

  pp::Rect dest;
  int rotate = CalculatePosition(page.get(), settings, &dest);

  ScopedFPDFBitmap bitmap(FPDFBitmap_CreateEx(
      settings.bounds.width(), settings.bounds.height(), FPDFBitmap_BGRA,
      bitmap_buffer, settings.bounds.width() * 4));
  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, settings.bounds.width(),
                      settings.bounds.height(), 0xFFFFFFFF);
  // Shift top-left corner of bounds to (0, 0) if it's not there.
  dest.set_point(dest.point() - settings.bounds.point());

  int flags = FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH;
  if (!settings.use_color)
    flags |= FPDF_GRAYSCALE;

  FPDF_RenderPageBitmap(bitmap.get(), page.get(), dest.x(), dest.y(),
                        dest.width(), dest.height(), rotate, flags);
  return true;
}

std::vector<uint8_t> PDFiumEngineExports::ConvertPdfPagesToNupPdf(
    std::vector<base::span<const uint8_t>> input_buffers,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  if (!IsValidPrintableArea(page_size, printable_area))
    return std::vector<uint8_t>();

  ScopedFPDFDocument doc = CreatePdfDoc(std::move(input_buffers));
  if (!doc)
    return std::vector<uint8_t>();

  return PDFiumPrint::CreateNupPdf(std::move(doc), pages_per_sheet, page_size,
                                   printable_area);
}

std::vector<uint8_t> PDFiumEngineExports::ConvertPdfDocumentToNupPdf(
    base::span<const uint8_t> input_buffer,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  if (!IsValidPrintableArea(page_size, printable_area))
    return std::vector<uint8_t>();

  ScopedFPDFDocument doc = LoadPdfData(input_buffer);
  if (!doc)
    return std::vector<uint8_t>();

  return PDFiumPrint::CreateNupPdf(std::move(doc), pages_per_sheet, page_size,
                                   printable_area);
}

bool PDFiumEngineExports::GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                                        int* page_count,
                                        double* max_page_width) {
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;

  if (!page_count && !max_page_width)
    return true;

  int page_count_local = FPDF_GetPageCount(doc.get());
  if (page_count)
    *page_count = page_count_local;

  if (max_page_width) {
    *max_page_width = 0;
    for (int page_number = 0; page_number < page_count_local; page_number++) {
      double page_width = 0;
      double page_height = 0;
      FPDF_GetPageSizeByIndex(doc.get(), page_number, &page_width,
                              &page_height);
      if (page_width > *max_page_width) {
        *max_page_width = page_width;
      }
    }
  }
  return true;
}

bool PDFiumEngineExports::GetPDFPageSizeByIndex(
    base::span<const uint8_t> pdf_buffer,
    int page_number,
    double* width,
    double* height) {
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;
  return FPDF_GetPageSizeByIndex(doc.get(), page_number, width, height) != 0;
}

}  // namespace chrome_pdf
