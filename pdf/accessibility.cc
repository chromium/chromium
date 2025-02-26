// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "pdf/accessibility_helper.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

AccessibilityFormFieldInfo GetAccessibilityFormFieldInfo(
    PDFiumPage* page,
    uint32_t text_run_count) {
  AccessibilityFormFieldInfo form_field_info;
  form_field_info.text_fields = page->GetTextFieldInfo(text_run_count);
  return form_field_info;
}

}  // namespace

void GetAccessibilityInfo(PDFiumEngine* engine,
                          int32_t page_index,
                          AccessibilityPageInfo& page_info,
                          std::vector<AccessibilityTextRunInfo>& text_runs,
                          std::vector<AccessibilityCharInfo>& chars,
                          AccessibilityPageObjects& page_objects) {
  PDFiumPage* page = engine->GetPage(page_index);
  CHECK(page);

  const int raw_char_count = page->GetCharCount();
  // Treat a char count of -1 (error) as 0 (an empty page), since
  // other pages might have valid content.
  const uint32_t char_count = std::max<uint32_t>(raw_char_count, 0);

  page_info.page_index = page_index;
  page_info.bounds = page->rect();
  page_info.char_count = char_count;
  page_info.is_searchified = page->IsPageSearchified();
  page->GetTextAndImageInfo(text_runs, chars, page_objects.images);
  page_info.text_run_count = text_runs.size();
  page_objects.links = page->GetLinkInfo(text_runs);
  page_objects.highlights = page->GetHighlightInfo(text_runs);
  page_objects.form_fields =
      GetAccessibilityFormFieldInfo(page, page_info.text_run_count);
}

}  // namespace chrome_pdf
