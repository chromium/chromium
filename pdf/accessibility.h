// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_H_
#define PDF_ACCESSIBILITY_H_

#include <stdint.h>

#include <vector>

#include "ppapi/cpp/private/pdf.h"

namespace chrome_pdf {

class PDFEngine;

// Retrieve |page_info|, |text_runs|, |chars|, and |page_objects| from
// |engine| for the page at 0-indexed |page_index|. Returns true on success with
// all out parameters filled, or false on failure with all out parameters
// untouched.
bool GetAccessibilityInfo(
    PDFEngine* engine,
    int32_t page_index,
    PP_PrivateAccessibilityPageInfo* page_info,
    std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>* text_runs,
    std::vector<PP_PrivateAccessibilityCharInfo>* chars,
    pp::PDF::PrivateAccessibilityPageObjects* page_objects);

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_H_
