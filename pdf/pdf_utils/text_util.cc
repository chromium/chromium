// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_utils/text_util.h"

#include <stdint.h>

#include "base/strings/string_util.h"
#include "pdf/pdfium/pdfium_range.h"

namespace chrome_pdf {

bool IsWordBoundary(uint32_t ch) {
  // Deal with ASCII characters.
  if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '_') {
    return false;
  }
  return ch < 128 || ch == kZeroWidthSpace;
}

}  // namespace chrome_pdf
