// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_UTILS_DATES_H_
#define PDF_PDF_UTILS_DATES_H_

#include "base/strings/string_piece.h"

namespace base {
class Time;
}  // namespace base

namespace chrome_pdf {

// Parses a string in the PDF date format (see section 7.9.4 "Dates" of the ISO
// 32000-1:2008 spec). If `date` cannot be parsed, returns a "null" time (one
// for which `base::Time::is_null()` returns `true`).
base::Time ParsePdfDate(base::StringPiece date);

}  // namespace chrome_pdf

#endif  // PDF_PDF_UTILS_DATES_H_
