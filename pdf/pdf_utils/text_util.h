// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_UTILS_TEXT_UTIL_H_
#define PDF_PDF_UTILS_TEXT_UTIL_H_

#include <stdint.h>

namespace chrome_pdf {

// Returns whether `ch` is a word boundary.
bool IsWordBoundary(uint32_t ch);

}  // namespace chrome_pdf

#endif  // PDF_PDF_UTILS_TEXT_UTIL_H_
