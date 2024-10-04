// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_init.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>

#include "base/functional/callback.h"
#include "pdf/pdf_progressive_searchifier.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace chrome_pdf {

namespace {

std::optional<bool> g_use_skia_renderer_enabled_by_policy;

class ScopedSdkInitializer {
 public:
  explicit ScopedSdkInitializer(bool enable_v8) {
    CHECK(!IsSDKInitializedViaPlugin());
    InitializeSDK(
        enable_v8,
        g_use_skia_renderer_enabled_by_policy.value_or(
            base::FeatureList::IsEnabled(features::kPdfUseSkiaRenderer)),
        FontMappingMode::kNoMapping);
  }

  ScopedSdkInitializer(const ScopedSdkInitializer&) = delete;
  ScopedSdkInitializer& operator=(const ScopedSdkInitializer&) = delete;

  ~ScopedSdkInitializer() {
    CHECK(!IsSDKInitializedViaPlugin());
    ShutdownSDK();
  }
};

}  // namespace

void SetUseSkiaRendererPolicy(bool use_skia) {
  g_use_skia_renderer_enabled_by_policy = use_skia;
}

#if BUILDFLAG(IS_CHROMEOS)
std::optional<FlattenPdfResult> CreateFlattenedPdf(
    base::span<const uint8_t> input_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  return PDFiumEngineExports::Get()->CreateFlattenedPdf(input_buffer);
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
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  PDFiumEngineExports::RenderingSettings settings(
      gfx::Size(dpi_x, dpi_y),
      gfx::Rect(bounds_origin_x, bounds_origin_y, bounds_width, bounds_height),
      fit_to_bounds, stretch_to_bounds, keep_aspect_ratio, center_in_bounds,
      autorotate, use_color, /*render_for_printing=*/true);
  return engine_exports->RenderPDFPageToDC(pdf_buffer, page_index, settings,
                                           dc);
}

void SetPDFUsePrintMode(int mode) {
  PDFiumEngineExports::Get()->SetPDFUsePrintMode(mode);
}
#endif  // BUILDFLAG(IS_WIN)

bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                   int* page_count,
                   float* max_page_width) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->GetPDFDocInfo(pdf_buffer, page_count, max_page_width);
}

std::optional<DocumentMetadata> GetPDFDocMetadata(
    base::span<const uint8_t> pdf_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->GetPDFDocMetadata(pdf_buffer);
}

std::optional<bool> IsPDFDocTagged(base::span<const uint8_t> pdf_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->IsPDFDocTagged(pdf_buffer);
}

base::Value GetPDFStructTreeForPage(base::span<const uint8_t> pdf_buffer,
                                    int page_index) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->GetPDFStructTreeForPage(pdf_buffer, page_index);
}

std::optional<bool> PDFDocHasOutline(base::span<const uint8_t> pdf_buffer) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->PDFDocHasOutline(pdf_buffer);
}

std::optional<gfx::SizeF> GetPDFPageSizeByIndex(
    base::span<const uint8_t> pdf_buffer,
    int page_index) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  chrome_pdf::PDFiumEngineExports* engine_exports =
      chrome_pdf::PDFiumEngineExports::Get();
  return engine_exports->GetPDFPageSizeByIndex(pdf_buffer, page_index);
}

bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                           int page_index,
                           void* bitmap_buffer,
                           const gfx::Size& bitmap_size,
                           const gfx::Size& dpi,
                           const RenderOptions& options) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/true);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  PDFiumEngineExports::RenderingSettings settings(
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
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->ConvertPdfPagesToNupPdf(
      std::move(input_buffers), pages_per_sheet, page_size, printable_area);
}

std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
    base::span<const uint8_t> input_buffer,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->ConvertPdfDocumentToNupPdf(
      input_buffer, pages_per_sheet, page_size, printable_area);
}

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
std::vector<uint8_t> Searchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback) {
  ScopedSdkInitializer scoped_sdk_initializer(/*enable_v8=*/false);
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->Searchify(pdf_buffer, std::move(perform_ocr_callback));
}

std::unique_ptr<PdfProgressiveSearchifier> CreateProgressiveSearchifier() {
  PDFiumEngineExports* engine_exports = PDFiumEngineExports::Get();
  return engine_exports->CreateProgressiveSearchifier();
}
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace chrome_pdf
