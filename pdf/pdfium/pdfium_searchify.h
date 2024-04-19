// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_SEARCHIFY_H_
#define PDF_PDFIUM_PDFIUM_SEARCHIFY_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chrome_pdf {

std::vector<uint8_t> PDFiumSearchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_SEARCHIFY_H_
