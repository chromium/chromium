// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf.h"

#include <stdint.h>

#include <utility>

#include "build/build_config.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdf_init.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

namespace {

class ScopedSdkInitializer {
 public:
  explicit ScopedSdkInitializer(bool enable_v8) {
    if (!IsSDKInitializedViaPlugin())
      InitializeSDK(enable_v8, FontMappingMode::kNoMapping);
  }

  ScopedSdkInitializer(const ScopedSdkInitializer&) = delete;
  ScopedSdkInitializer& operator=(const ScopedSdkInitializer&) = delete;

  ~ScopedSdkInitializer() {
    if (!IsSDKInitializedViaPlugin())
      ShutdownSDK();
  }
};

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
std::vector<uint8_t> CreateFlattenedPdf(
    base::span<const uint8_t> input_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  return PDFEngineExports::Get()->CreateFlattenedPdf(input_buffer);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                       int page_index,
                       HDC dc,
                       int dpi_x,
                       int dpi_y,
                       int bounds_origin_x,
                       int bounds_origin_y,
                       int bounds_width,
                       int bounds_height,
                       bool fit_to_bounds,
                       bool stretch_to_bounds,
                       bool keep_aspect_ratio,
                       bool center_in_bounds,
                       bool autorotate,
                       bool use_color) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  PDFEngineExports::RenderingSettings settings(
      gfx::Size(dpi_x, dpi_y),
      gfx::Rect(bounds_origin_x, bounds_origin_y, bounds_width, bounds_height),
      fit_to_bounds, stretch_to_bounds, keep_aspect_ratio, center_in_bounds,
      autorotate, use_color, /*render_for_printing=*/true);
  return engine_exports->RenderPDFPageToDC(pdf_buffer, page_index, settings,
                                           dc);
}

void SetPDFUsePrintMode(int mode) {
  PDFEngineExports::Get()->SetPDFUsePrintMode(mode);
}
#endif  // BUILDFLAG(IS_WIN)

bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                   int* page_count,
                   float* max_page_width) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  return engine_exports->GetPDFDocInfo(pdf_buffer, page_count, max_page_width);
}

absl::optional<bool> IsPDFDocTagged(base::span<const uint8_t> pdf_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  return engine_exports->IsPDFDocTagged(pdf_buffer);
}

base::Value GetPDFStructTreeForPage(base::span<const uint8_t> pdf_buffer,
                                    int page_index) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  return engine_exports->GetPDFStructTreeForPage(pdf_buffer, page_index);
}

absl::optional<gfx::SizeF> GetPDFPageSizeByIndex(
    base::span<const uint8_t> pdf_buffer,
    int page_index) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  chrome_pdf::PDFEngineExports* engine_exports =
      chrome_pdf::PDFEngineExports::Get();
  return engine_exports->GetPDFPageSizeByIndex(pdf_buffer, page_index);
}

bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                           int page_index,
                           void* bitmap_buffer,
                           const gfx::Size& bitmap_size,
                           const gfx::Size& dpi,
                           const RenderOptions& options) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  PDFEngineExports::RenderingSettings settings(
      dpi, gfx::Rect(bitmap_size),
      /*fit_to_bounds=*/true, options.stretch_to_bounds,
      options.keep_aspect_ratio,
      /*center_in_bounds=*/true, options.autorotate, options.use_color,
      options.render_device_type == RenderDeviceType::kPrinter);
  return engine_exports->RenderPDFPageToBitmap(pdf_buffer, page_index, settings,
                                               bitmap_buffer);
}

std::vector<uint8_t> ConvertPdfPagesToNupPdf(
    std::vector<base::span<const uint8_t>> input_buffers,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  return engine_exports->ConvertPdfPagesToNupPdf(
      std::move(input_buffers), pages_per_sheet, page_size, printable_area);
}

std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
    base::span<const uint8_t> input_buffer,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  PDFEngineExports* engine_exports = PDFEngineExports::Get();
  return engine_exports->ConvertPdfDocumentToNupPdf(
      input_buffer, pages_per_sheet, page_size, printable_area);
}

}  // namespace chrome_pdf
