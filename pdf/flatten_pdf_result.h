// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_FLATTEN_PDF_RESULT_H_
#define PDF_FLATTEN_PDF_RESULT_H_

#include <cstdint>
#include <vector>

namespace chrome_pdf {

struct FlattenPdfResult {
  FlattenPdfResult(std::vector<uint8_t> pdf_in, uint32_t page_count);
  FlattenPdfResult(const FlattenPdfResult&);
  FlattenPdfResult& operator=(const FlattenPdfResult&);
  FlattenPdfResult(FlattenPdfResult&&) noexcept;
  FlattenPdfResult& operator=(FlattenPdfResult&&) noexcept;
  ~FlattenPdfResult();

  // `pdf` is never empty.
  std::vector<uint8_t> pdf;

  // `page_count` is always strictly greater than zero.
  uint32_t page_count;
};

}  // namespace chrome_pdf

#endif  // PDF_FLATTEN_PDF_RESULT_H_
