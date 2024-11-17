// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_api_wrappers.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_WIN)
#include <string.h>  // for memset()
#endif

using printing::ConvertUnitFloat;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

int GetRenderFlagsFromSettings(
    const PDFiumEngineExports::RenderingSettings& settings) {
  int flags = FPDF_ANNOT;
  if (!settings.use_color) {
    flags |= FPDF_GRAYSCALE;
  }
  if (settings.render_for_printing) {
    flags |= FPDF_PRINTING;
  }
  return flags;
}

int CalculatePosition(FPDF_PAGE page,
                      const PDFiumEngineExports::RenderingSettings& settings,
                      gfx::Rect* dest) {
  // settings.bounds is in terms of the max DPI. Convert page sizes to match.
  const int dpi_x = settings.dpi.width();
  const int dpi_y = settings.dpi.height();
  const int dpi = std::max(dpi_x, dpi_y);
  int page_width = static_cast<int>(
      ConvertUnitFloat(FPDF_GetPageWidthF(page), kPointsPerInch, dpi));
  int page_height = static_cast<int>(
      ConvertUnitFloat(FPDF_GetPageHeightF(page), kPointsPerInch, dpi));

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
  if (dpi_x != dpi_y) {
    dest->set_width(dest->width() * dpi_x / dpi);
    dest->set_height(dest->height() * dpi_y / dpi);
  }

  if (settings.center_in_bounds) {
    gfx::Vector2d offset(
        (settings.bounds.width() * dpi_x / dpi - dest->width()) / 2,
        (settings.bounds.height() * dpi_y / dpi - dest->height()) / 2);
    dest->Offset(offset);
  }
  return rotate;
}

}  // namespace

ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_data) {
  return LoadPdfDataWithPassword(pdf_data, std::string());
}

ScopedFPDFDocument LoadPdfDataWithPassword(base::span<const uint8_t> pdf_data,
                                           const std::string& password) {
  return ScopedFPDFDocument(FPDF_LoadMemDocument64(
      pdf_data.data(), pdf_data.size(), password.c_str()));
}

std::u16string GetPageObjectMarkName(FPDF_PAGEOBJECTMARK mark) {
  // FPDFPageObjMark_GetName() naturally handles null `mark` inputs, so no
  // explicit check.

  std::u16string name;
  // NOLINT used below because this is required by the PDFium API interaction.
  unsigned long buflen_bytes = 0;  // NOLINT(runtime/int)
  if (!FPDFPageObjMark_GetName(mark, nullptr, 0, &buflen_bytes)) {
    return name;
  }

  // PDFium should never return an odd number of bytes for 16-bit chars.
  static_assert(sizeof(FPDF_WCHAR) == sizeof(char16_t));
  CHECK_EQ(buflen_bytes % 2, 0u);

  // Number of characters, including the NUL.
  const size_t expected_size = base::checked_cast<size_t>(buflen_bytes / 2);
  PDFiumAPIStringBufferAdapter adapter(&name, expected_size,
                                       /*check_expected_size=*/true);
  unsigned long actual_buflen_bytes = 0;  // NOLINT(runtime/int)
  bool result =
      FPDFPageObjMark_GetName(mark, static_cast<FPDF_WCHAR*>(adapter.GetData()),
                              buflen_bytes, &actual_buflen_bytes);
  CHECK(result);

  // Reuse `expected_size`, as `actual_buflen_bytes` divided by 2 is equal.
  CHECK_EQ(actual_buflen_bytes, buflen_bytes);
  adapter.Close(expected_size);
  return name;
}

bool RenderPageToBitmap(FPDF_PAGE page,
                        const PDFiumEngineExports::RenderingSettings& settings,
                        void* bitmap_buffer) {
  if (!page || !bitmap_buffer) {
    return false;
  }

  constexpr int kBgraImageColorChannels = 4;
  base::CheckedNumeric<int> stride = kBgraImageColorChannels;
  stride *= settings.bounds.width();
  if (!stride.IsValid()) {
    return false;
  }

  gfx::Rect dest;
  int rotate = CalculatePosition(page, settings, &dest);

  ScopedFPDFBitmap bitmap(
      FPDFBitmap_CreateEx(settings.bounds.width(), settings.bounds.height(),
                          FPDFBitmap_BGRA, bitmap_buffer, stride.ValueOrDie()));
  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, settings.bounds.width(),
                      settings.bounds.height(), 0xFFFFFFFF);
  // Shift top-left corner of bounds to (0, 0) if it's not there.
  dest.set_origin(dest.origin() - settings.bounds.OffsetFromOrigin());

  FPDF_RenderPageBitmap(bitmap.get(), page, dest.x(), dest.y(), dest.width(),
                        dest.height(), rotate,
                        GetRenderFlagsFromSettings(settings));
  return true;
}

#if BUILDFLAG(IS_WIN)
bool RenderPageToDC(FPDF_PAGE page,
                    const PDFiumEngineExports::RenderingSettings& settings,
                    HDC dc) {
  if (!page || !dc) {
    return false;
  }

  PDFiumEngineExports::RenderingSettings new_settings = settings;
  // calculate the page size
  if (new_settings.dpi.width() == -1) {
    new_settings.dpi.set_width(GetDeviceCaps(dc, LOGPIXELSX));
  }
  if (new_settings.dpi.height() == -1) {
    new_settings.dpi.set_height(GetDeviceCaps(dc, LOGPIXELSY));
  }

  gfx::Rect dest;
  int rotate = CalculatePosition(page, new_settings, &dest);

  int save_state = SaveDC(dc);
  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  IntersectClipRect(dc, settings.bounds.x(), settings.bounds.y(),
                    settings.bounds.x() + settings.bounds.width(),
                    settings.bounds.y() + settings.bounds.height());

  int flags = GetRenderFlagsFromSettings(settings);

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
    FPDF_RenderPageBitmap(bitmap.get(), page, 0, 0, dest.width(), dest.height(),
                          rotate, flags);
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
    FPDF_RenderPage(dc, page, dest.x(), dest.y(), dest.width(), dest.height(),
                    rotate, flags);
  }
  RestoreDC(dc, save_state);
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace chrome_pdf
