// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_H_
#define PDF_PDF_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/document_metadata.h"
#include "services/screen_ai/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "pdf/flatten_pdf_result.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>

#include "base/functional/callback_forward.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace gfx {
class Rect;
class Size;
class SizeF;
}  // namespace gfx

namespace chrome_pdf {

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class PdfProgressiveSearchifier;
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void SetUseSkiaRendererPolicy(bool use_skia);

#if BUILDFLAG(IS_CHROMEOS)
// Create a flattened PDF document from an existing PDF document.
// `input_buffer` is the buffer that contains the entire PDF document to be
// flattened.
std::optional<FlattenPdfResult> CreateFlattenedPdf(
    base::span<const uint8_t> input_buffer);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// Printing modes - type to convert PDF to for printing. See PDFium's
// FPDF_SetPrintMode() for details.
enum PrintingMode {
  kEmf = 0,
  kTextOnly = 1,
  kPostScript2 = 2,
  kPostScript3 = 3,
  // Values 4 and 5 are similar to `kPostScript2` and `kPostScript3`, but are
  // not intended for use in sandboxed environments like Chromium's.
  kEmfWithReducedRasterization = 6,
  kPostScript3WithType42Fonts = 7,
  // Value 8 is similar to `kPostScript3WithType42Fonts`, but is not intended
  // for use in sandboxed environments like Chromium's.
};

// `pdf_buffer` is the buffer that contains the entire PDF document to be
//     rendered.
// `page_index` is the 0-based index of the page to be rendered.
// `dc` is the device context to render into.
// `dpi_x` and `dpi_y` is the resolution.
// `bounds_origin_x`, `bounds_origin_y`, `bounds_width` and `bounds_height`
//     specify a bounds rectangle within the DC in which to render the PDF
//     page.
// `fit_to_bounds` specifies whether the output should be shrunk to fit the
//     supplied bounds if the page size is larger than the bounds in any
//     dimension. If this is false, parts of the PDF page that lie outside
//     the bounds will be clipped.
// `stretch_to_bounds` specifies whether the output should be stretched to fit
//     the supplied bounds if the page size is smaller than the bounds in any
//     dimension.
// If both `fit_to_bounds` and `stretch_to_bounds` are true, then
//     `fit_to_bounds` is honored first.
// `keep_aspect_ratio` If any scaling is to be done is true, this flag
//     specifies whether the original aspect ratio of the page should be
//     preserved while scaling.
// `center_in_bounds` specifies whether the final image (after any scaling is
//     done) should be centered within the given bounds.
// `autorotate` specifies whether the final image should be rotated to match
//     the output bound.
// `use_color` specifies color or grayscale.
// Returns false if the document or the page number are not valid.
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
                       bool use_color);

void SetPDFUsePrintMode(int mode);
#endif  // BUILDFLAG(IS_WIN)

// `page_count` and `max_page_width` are optional and can be NULL.
// Returns false if the document is not valid.
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

// Whether the PDF has a Document Outline (see ISO 32000-1:2008 12.3.3 "Document
// Outline"). Returns true if the PDF has an outline, false if it's a valid PDF
// without an outline, and nullopt if the PDF can't be parsed.
std::optional<bool> PDFDocHasOutline(base::span<const uint8_t> pdf_buffer);

// Gets the dimensions of a specific page in a document.
// `pdf_buffer` is the buffer that contains the entire PDF document to be
//     rendered.
// `page_index` is the page number that the function will get the dimensions of.
// Returns the size of the page in points, or nullopt if the document or the
// page number are not valid.
std::optional<gfx::SizeF> GetPDFPageSizeByIndex(
    base::span<const uint8_t> pdf_buffer,
    int page_index);

enum class RenderDeviceType {
  kDisplay,
  kPrinter,
};

struct RenderOptions {
  // Whether the output should be stretched to fit the supplied bitmap.
  bool stretch_to_bounds;
  // If any scaling is needed, whether the original aspect ratio of the page is
  // preserved while scaling.
  bool keep_aspect_ratio;
  // Whether the final image should be rotated to match the output bound.
  bool autorotate;
  // Specifies color or grayscale.
  bool use_color;
  // What type of device to render for.
  RenderDeviceType render_device_type;
};

// Renders PDF page into 4-byte per pixel BGRA color bitmap.
// `pdf_buffer` is the buffer that contains the entire PDF document to be
//     rendered.
// `page_index` is the 0-based index of the page to be rendered.
// `bitmap_buffer` is the output buffer for bitmap.
// `bitmap_size` is the size of the output bitmap.
// `dpi` is the 2D resolution.
// `options` is the options to render with.
// Returns false if the document or the page number are not valid.
bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                           int page_index,
                           void* bitmap_buffer,
                           const gfx::Size& bitmap_size,
                           const gfx::Size& dpi,
                           const RenderOptions& options);

// Convert multiple PDF pages into a N-up PDF.
// `input_buffers` is the vector of buffers with each buffer contains a PDF.
//     If any of the PDFs contains multiple pages, only the first page of the
//     document is used.
// `pages_per_sheet` is the number of pages to put on one sheet.
// `page_size` is the output page size, measured in PDF "user space" units.
// `printable_area` is the output page printable area, measured in PDF
//     "user space" units.  Should be smaller than `page_size`.
//
// `page_size` is the print media size.  The page size of the output N-up PDF is
// determined by the `pages_per_sheet`, the orientation of the PDF pages
// contained in the `input_buffers`, and the media page size `page_size`. For
// example, when `page_size` = 512x792, `pages_per_sheet` = 2, and the
// orientation of `input_buffers` = portrait, the output N-up PDF will be
// 792x512.
// See printing::NupParameters for more details on how the output page
// orientation is determined, to understand why `page_size` may be swapped in
// some cases.
std::vector<uint8_t> ConvertPdfPagesToNupPdf(
    std::vector<base::span<const uint8_t>> input_buffers,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area);

// Convert a PDF document to a N-up PDF document.
// `input_buffer` is the buffer that contains the entire PDF document to be
//     converted to a N-up PDF document.
// `pages_per_sheet` is the number of pages to put on one sheet.
// `page_size` is the output page size, measured in PDF "user space" units.
// `printable_area` is the output page printable area, measured in PDF
//     "user space" units.  Should be smaller than `page_size`.
//
// Refer to the description of ConvertPdfPagesToNupPdf to understand how the
// output page size will be calculated.
// The algorithm used to determine the output page size is the same.
std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
    base::span<const uint8_t> input_buffer,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area);

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// Converts an inaccessible PDF to a searchable PDF.
// `pdf_buffer` is the buffer of the inaccessible PDF.
// `perform_ocr_callback` is the callback that takes an image and outputs
//     the OCR result. It may be called multiple times.
//
// The conversion is done by performing OCR on each image in the PDF and adding
// a layer of invisible text to the PDF to make text on images accessible. Each
// execution should take place in an isolated process, and each process should
// be terminated upon completion of the conversion. An empty vector is returned
// on failure.
std::vector<uint8_t> Searchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback);

// Creates a PDF searchifier for future operations, such as adding and deleting
// pages, and saving PDFs. Crashes if failed to create.
std::unique_ptr<PdfProgressiveSearchifier> CreateProgressiveSearchifier();
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace chrome_pdf

#endif  // PDF_PDF_H_
