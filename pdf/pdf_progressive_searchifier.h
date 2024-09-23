// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_
#define PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_

#include <cstdint>
#include <vector>

#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chrome_pdf {

// Creates a PDF and provides operations to add and delete pages, and save the
// searchified PDF. The operation requests are handled one by one.
class PdfProgressiveSearchifier {
 public:
  virtual ~PdfProgressiveSearchifier() = default;

  // Adds a new page to the PDF at `page_index` with the given image and a layer
  // of invisible text. If the page already exists, it will be replaced. If
  // `page_index` is larger than PDF's current last index(L), the created page
  // index is the next available index(L+1).
  virtual void AddPage(const SkBitmap& bitmap,
                       uint32_t page_index,
                       screen_ai::mojom::VisualAnnotationPtr annotation) = 0;
  // Deletes the page of the PDF at `page_index` and shift the following pages
  // forward. Does nothing if the page at `page_index` doesn't exist.
  virtual void DeletePage(uint32_t page_index) = 0;
  // Returns the searchified PDF. It can be called multiple times at any time.
  virtual std::vector<uint8_t> Save() = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_
