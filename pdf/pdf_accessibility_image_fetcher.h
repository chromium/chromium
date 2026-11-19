// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_
#define PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_

#include <stdint.h>

class SkBitmap;

namespace chrome_pdf {

class PdfAccessibilityImageFetcher {
 public:
  virtual ~PdfAccessibilityImageFetcher() = default;

  // Fetches the image as a 32-bit bitmap with width and height at most
  // `max_image_dimension`.
  virtual SkBitmap GetImageBitmap(uint32_t page_index,
                                  int32_t page_object_index,
                                  uint32_t max_image_dimension) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_
