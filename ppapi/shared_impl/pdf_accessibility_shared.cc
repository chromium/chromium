// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/pdf_accessibility_shared.h"

namespace ppapi {

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo() = default;

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo(
    const PP_PrivateAccessibilityTextStyleInfo& style)
    : font_name(std::string(style.font_name, style.font_name_length)),
      font_weight(style.font_weight),
      render_mode(style.render_mode),
      font_size(style.font_size),
      fill_color(style.fill_color),
      stroke_color(style.stroke_color),
      is_italic(style.is_italic),
      is_bold(style.is_bold) {}

PdfAccessibilityTextStyleInfo::PdfAccessibilityTextStyleInfo(
    PdfAccessibilityTextStyleInfo&& other) = default;

PdfAccessibilityTextStyleInfo::~PdfAccessibilityTextStyleInfo() = default;

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo() = default;

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo(
    const PP_PrivateAccessibilityTextRunInfo& text_run)
    : len(text_run.len),
      bounds(text_run.bounds),
      direction(text_run.direction),
      style(text_run.style) {}

PdfAccessibilityTextRunInfo::PdfAccessibilityTextRunInfo(
    PdfAccessibilityTextRunInfo&& other) = default;

PdfAccessibilityTextRunInfo::~PdfAccessibilityTextRunInfo() = default;

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo() = default;

PdfAccessibilityLinkInfo::PdfAccessibilityLinkInfo(
    const PP_PrivateAccessibilityLinkInfo& link)
    : url(std::string(link.url, link.url_length)),
      index_in_page(link.index_in_page),
      text_run_index(link.text_run_index),
      text_run_count(link.text_run_count),
      bounds(link.bounds) {}

PdfAccessibilityLinkInfo::~PdfAccessibilityLinkInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo() = default;

PdfAccessibilityImageInfo::PdfAccessibilityImageInfo(
    const PP_PrivateAccessibilityImageInfo& image)
    : alt_text(std::string(image.alt_text, image.alt_text_length)),
      text_run_index(image.text_run_index),
      bounds(image.bounds) {}

PdfAccessibilityImageInfo::~PdfAccessibilityImageInfo() = default;

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects() = default;

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects(
    const PP_PrivateAccessibilityPageObjects& page_objects) {
  links.reserve(page_objects.link_count);
  for (size_t i = 0; i < page_objects.link_count; i++) {
    links.emplace_back(page_objects.links[i]);
  }

  images.reserve(page_objects.image_count);
  for (size_t i = 0; i < page_objects.image_count; i++) {
    images.emplace_back(page_objects.images[i]);
  }
}

PdfAccessibilityPageObjects::~PdfAccessibilityPageObjects() = default;

}  // namespace ppapi
