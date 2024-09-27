// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_SEARCHIFY_H_
#define PDF_PDFIUM_PDFIUM_SEARCHIFY_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chrome_pdf {

using PerformOcrCallbackAsync = base::RepeatingCallback<void(
    const SkBitmap& bitmap,
    base::OnceCallback<void(
        screen_ai::mojom::VisualAnnotationPtr annotation)>)>;

struct SearchifyBoundingBoxOrigin {
  gfx::PointF point;
  float theta;
};

std::vector<uint8_t> PDFiumSearchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback);

// Creates an invisible font.
ScopedFPDFFont CreateFont(FPDF_DOCUMENT document);

// Adds the recognized text in `annotation` to the given `page`, to be written
// over `image`.
void AddTextOnImage(FPDF_DOCUMENT document,
                    FPDF_PAGE page,
                    FPDF_FONT font,
                    FPDF_PAGEOBJECT image,
                    screen_ai::mojom::VisualAnnotationPtr annotation,
                    const gfx::Size& image_pixel_size);

// Internal functions exposed for testing.
SearchifyBoundingBoxOrigin ConvertToPdfOriginForTesting(
    const gfx::Rect& rect,
    float angle,
    float coordinate_system_height);
FS_MATRIX CalculateWordMoveMatrixForTesting(
    const SearchifyBoundingBoxOrigin& origin,
    int word_bounding_box_width,
    bool word_is_rtl);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_SEARCHIFY_H_
