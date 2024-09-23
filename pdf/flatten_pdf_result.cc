// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "pdf/flatten_pdf_result.h"

#include "base/check_op.h"

namespace chrome_pdf {

FlattenPdfResult::FlattenPdfResult(std::vector<uint8_t> pdf_in,
                                   uint32_t page_count)
    : pdf(std::move(pdf_in)), page_count(page_count) {
  CHECK_GT(page_count, 0U);
  CHECK(!pdf.empty());
}

FlattenPdfResult::FlattenPdfResult(const FlattenPdfResult&) = default;

FlattenPdfResult& FlattenPdfResult::operator=(const FlattenPdfResult&) =
    default;

FlattenPdfResult::FlattenPdfResult(FlattenPdfResult&&) noexcept = default;

FlattenPdfResult& FlattenPdfResult::operator=(FlattenPdfResult&&) noexcept =
    default;

FlattenPdfResult::~FlattenPdfResult() = default;

}  // namespace chrome_pdf
