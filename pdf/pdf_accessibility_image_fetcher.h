// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_
#define PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_

class SkBitmap;

namespace chrome_pdf {

class PdfAccessibilityImageFetcher {
 public:
  virtual ~PdfAccessibilityImageFetcher() = default;
  // Fetches the image as a 32-bit bitmap for OCR.
  virtual SkBitmap GetImageForOcr(int32_t page_index,
                                  int32_t page_object_index) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_IMAGE_FETCHER_H_
