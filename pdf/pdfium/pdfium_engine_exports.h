// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/document_metadata.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "pdf/flatten_pdf_result.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>

#include "base/functional/callback_forward.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace chrome_pdf {

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class PdfProgressiveSearchifier;
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

// Interface for exports that wrap PDFiumEngine.
class PDFiumEngineExports {
 public:
  struct RenderingSettings {
    RenderingSettings(const gfx::Size& dpi,
                      const gfx::Rect& bounds,
                      bool fit_to_bounds,
                      bool stretch_to_bounds,
                      bool keep_aspect_ratio,
                      bool center_in_bounds,
                      bool autorotate,
                      bool use_color,
                      bool render_for_printing);
    RenderingSettings(const RenderingSettings& that);

    gfx::Size dpi;
    gfx::Rect bounds;
    bool fit_to_bounds;
    bool stretch_to_bounds;
    bool keep_aspect_ratio;
    bool center_in_bounds;
    bool autorotate;
    bool use_color;
    bool render_for_printing;
  };

  static PDFiumEngineExports* Get();

  PDFiumEngineExports();
  PDFiumEngineExports(const PDFiumEngineExports&) = delete;
  PDFiumEngineExports& operator=(const PDFiumEngineExports&) = delete;
  ~PDFiumEngineExports();

#if BUILDFLAG(IS_CHROMEOS)
  // See the definition of CreateFlattenedPdf in pdf.cc for details.
  std::optional<FlattenPdfResult> CreateFlattenedPdf(
      base::span<const uint8_t> input_buffer);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  // See the definition of RenderPDFPageToDC in pdf.cc for details.
  bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                         int page_index,
                         const RenderingSettings& settings,
                         HDC dc);

  void SetPDFUsePrintMode(int mode);
#endif  // BUILDFLAG(IS_WIN)

  // See the definition of RenderPDFPageToBitmap in pdf.cc for details.
  bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                             int page_index,
                             const RenderingSettings& settings,
                             void* bitmap_buffer);

  // See the definition of ConvertPdfPagesToNupPdf in pdf.cc for details.
  std::vector<uint8_t> ConvertPdfPagesToNupPdf(
      std::vector<base::span<const uint8_t>> input_buffers,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area);

  // See the definition of ConvertPdfDocumentToNupPdf in pdf.cc for details.
  std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
      base::span<const uint8_t> input_buffer,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area);

  bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                     int* page_count,
                     float* max_page_width);

  // Gets the PDF document metadata (see section 14.3.3 "Document Information
  // Dictionary" of the ISO 32000-1:2008 spec).
  std::optional<DocumentMetadata> GetPDFDocMetadata(
      base::span<const uint8_t> pdf_buffer);

  // Whether the PDF is Tagged (see ISO 32000-1:2008 14.8 "Tagged PDF").
  // Returns true if it's a tagged (accessible) PDF, false if it's a valid
  // PDF but untagged, and nullopt if the PDF can't be parsed.
  std::optional<bool> IsPDFDocTagged(base::span<const uint8_t> pdf_buffer);

  // Given a tagged PDF (see IsPDFDocTagged, above), return the portion of
  // the structure tree for a given page as a hierarchical tree of base::Values.
  base::Value GetPDFStructTreeForPage(base::span<const uint8_t> pdf_buffer,
                                      int page_index);

  // Whether the PDF has a Document Outline (see ISO 32000-1:2008 12.3.3
  // "Document Outline"). Returns true if the PDF has an outline, false if it's
  // a valid PDF without an outline, and nullopt if the PDF can't be parsed.
  std::optional<bool> PDFDocHasOutline(base::span<const uint8_t> pdf_buffer);

  // See the definition of GetPDFPageSizeByIndex in pdf.cc for details.
  std::optional<gfx::SizeF> GetPDFPageSizeByIndex(
      base::span<const uint8_t> pdf_buffer,
      int page_index);

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Converts an inaccessible PDF to a searchable PDF. See `Searchify` in pdf.h
  // for more details.
  std::vector<uint8_t> Searchify(
      base::span<const uint8_t> pdf_buffer,
      base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
          const SkBitmap& bitmap)> perform_ocr_callback);

  // Creates a PDF searchifier for future operations, such as adding and
  // deleting pages, and saving PDFs.
  std::unique_ptr<PdfProgressiveSearchifier> CreateProgressiveSearchifier();
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
