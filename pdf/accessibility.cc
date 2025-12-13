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

AccessibilityFormFieldInfo GetAccessibilityFormFieldInfo(PDFiumPage* page) {
  AccessibilityFormFieldInfo form_field_info;
  form_field_info.text_fields = page->GetTextFieldInfo();
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

  page->PopulateTextRunTypeAndImageAltText();
  text_runs = page->GetTextRunInfo();
  chars = page->GetCharInfo();

  page_info.page_index = page_index;
  page_info.bounds = page->rect();
  page_info.char_count = chars.size();
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  page_info.is_searchified = page->IsPageSearchified();
#else
  page_info.is_searchified = false;
#endif
  page_objects.images = page->GetImageInfo();
  page_info.text_run_count = text_runs.size();
  page_objects.links = page->GetLinkInfo();
  page_objects.highlights = page->GetHighlightInfo();
  page_objects.form_fields = GetAccessibilityFormFieldInfo(page);
}

}  // namespace chrome_pdf
