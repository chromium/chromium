// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAGE_CHARACTER_INDEX_H_
#define PDF_PAGE_CHARACTER_INDEX_H_

#include <stdint.h>

#include <compare>

namespace chrome_pdf {

struct PageCharacterIndex {
  friend constexpr auto operator<=>(const PageCharacterIndex&,
                                    const PageCharacterIndex&) = default;

  // Index of PDF page.
  uint32_t page_index = 0;
  // Index of character within the PDF page.
  uint32_t char_index = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PAGE_CHARACTER_INDEX_H_
