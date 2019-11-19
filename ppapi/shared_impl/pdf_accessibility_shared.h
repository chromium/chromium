// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_
#define PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_

#include <string>
#include <vector>

#include "ppapi/c/pp_rect.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Needs to stay in sync with PP_PrivateAccessibilityTextStyleInfo.
struct PPAPI_SHARED_EXPORT PdfAccessibilityTextStyleInfo {
 public:
  PdfAccessibilityTextStyleInfo();
  explicit PdfAccessibilityTextStyleInfo(
      const PP_PrivateAccessibilityTextStyleInfo& style);
  PdfAccessibilityTextStyleInfo(PdfAccessibilityTextStyleInfo&& other);
  ~PdfAccessibilityTextStyleInfo();

  std::string font_name;
  int font_weight;
  PP_TextRenderingMode render_mode;
  double font_size;
  // Colors are ARGB.
  uint32_t fill_color;
  uint32_t stroke_color;
  bool is_italic;
  bool is_bold;
};

// Needs to stay in sync with PP_PrivateAccessibilityTextRunInfo.
struct PPAPI_SHARED_EXPORT PdfAccessibilityTextRunInfo {
 public:
  PdfAccessibilityTextRunInfo();
  explicit PdfAccessibilityTextRunInfo(
      const PP_PrivateAccessibilityTextRunInfo& text_run);
  PdfAccessibilityTextRunInfo(PdfAccessibilityTextRunInfo&& other);
  ~PdfAccessibilityTextRunInfo();

  uint32_t len;
  struct PP_FloatRect bounds;
  PP_PrivateDirection direction;
  PdfAccessibilityTextStyleInfo style;
};

// Needs to stay in sync with PP_PrivateAccessibilityLinkInfo.
struct PPAPI_SHARED_EXPORT PdfAccessibilityLinkInfo {
  PdfAccessibilityLinkInfo();
  explicit PdfAccessibilityLinkInfo(
      const PP_PrivateAccessibilityLinkInfo& link);
  ~PdfAccessibilityLinkInfo();

  std::string url;
  uint32_t index_in_page;
  uint32_t text_run_index;
  uint32_t text_run_count;
  PP_FloatRect bounds;
};

// Needs to stay in sync with PP_PrivateAccessibilityImageInfo.
struct PPAPI_SHARED_EXPORT PdfAccessibilityImageInfo {
  PdfAccessibilityImageInfo();
  explicit PdfAccessibilityImageInfo(
      const PP_PrivateAccessibilityImageInfo& image);
  ~PdfAccessibilityImageInfo();

  std::string alt_text;
  uint32_t text_run_index;
  PP_FloatRect bounds;
};

// Needs to stay in sync with PP_PrivateAccessibilityPageObjects.
struct PPAPI_SHARED_EXPORT PdfAccessibilityPageObjects {
  PdfAccessibilityPageObjects();
  explicit PdfAccessibilityPageObjects(
      const PP_PrivateAccessibilityPageObjects& page_objects);
  ~PdfAccessibilityPageObjects();

  std::vector<PdfAccessibilityLinkInfo> links;
  std::vector<PdfAccessibilityImageInfo> images;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_
