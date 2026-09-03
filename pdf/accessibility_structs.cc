// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility_structs.h"

namespace chrome_pdf {

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo() = default;

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const std::string& font_name,
    int font_weight,
    AccessibilityTextRenderMode render_mode,
    float font_size,
    uint32_t fill_color,
    uint32_t stroke_color,
    bool is_italic)
    : font_name(font_name),
      font_weight(font_weight),
      render_mode(render_mode),
      font_size(font_size),
      fill_color(fill_color),
      stroke_color(stroke_color),
      is_italic(is_italic) {}

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const AccessibilityTextStyleInfo& other) = default;

AccessibilityTextStyleInfo::~AccessibilityTextStyleInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    uint32_t start_index,
    uint32_t len,
    const gfx::RectF& bounds,
    AccessibilityTextDirection direction,
    const AccessibilityTextStyleInfo& style)
    : AccessibilityTextRunInfo(start_index,
                               len,
                               bounds,
                               direction,
                               style,
                               /*is_searchified=*/false) {}

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    uint32_t start_index,
    uint32_t len,
    const gfx::RectF& bounds,
    AccessibilityTextDirection direction,
    const AccessibilityTextStyleInfo& style,
    bool is_searchified)
    : start_index(start_index),
      len(len),
      bounds(bounds),
      direction(direction),
      style(style),
      is_searchified(is_searchified) {}

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    const AccessibilityTextRunInfo& other) = default;

AccessibilityTextRunInfo::~AccessibilityTextRunInfo() = default;

AccessibilityImageInfo::AccessibilityImageInfo() = default;

AccessibilityImageInfo::AccessibilityImageInfo(const std::string& alt_text,
                                               uint32_t text_run_index,
                                               const gfx::RectF& bounds,
                                               int32_t page_object_index)
    : alt_text(alt_text),
      text_run_index(text_run_index),
      bounds(bounds),
      page_object_index(page_object_index) {}

AccessibilityImageInfo::AccessibilityImageInfo(
    const AccessibilityImageInfo& other) = default;

AccessibilityImageInfo::~AccessibilityImageInfo() = default;

AccessibilityStructureElement::AccessibilityStructureElement() = default;

AccessibilityStructureElement::~AccessibilityStructureElement() = default;

AccessibilityDocInfo::AccessibilityDocInfo() = default;

AccessibilityDocInfo::~AccessibilityDocInfo() = default;

AccessibilityLinkInfo::AccessibilityLinkInfo() = default;

AccessibilityLinkInfo::AccessibilityLinkInfo(
    const std::string& url,
    uint32_t index_in_page,
    const gfx::RectF& bounds,
    const AccessibilityTextRunRangeInfo& text_range)
    : url(url),
      index_in_page(index_in_page),
      bounds(bounds),
      text_range(text_range) {}

AccessibilityLinkInfo::AccessibilityLinkInfo(
    const AccessibilityLinkInfo& other) = default;

AccessibilityLinkInfo::~AccessibilityLinkInfo() = default;

AccessibilityHighlightInfo::AccessibilityHighlightInfo() = default;

AccessibilityHighlightInfo::AccessibilityHighlightInfo(
    const std::string& note_text,
    uint32_t index_in_page,
    uint32_t color,
    const gfx::RectF& bounds,
    const AccessibilityTextRunRangeInfo& text_range)
    : note_text(note_text),
      index_in_page(index_in_page),
      color(color),
      bounds(bounds),
      text_range(text_range) {}

AccessibilityHighlightInfo::AccessibilityHighlightInfo(
    const AccessibilityHighlightInfo& other) = default;

AccessibilityHighlightInfo::~AccessibilityHighlightInfo() = default;

AccessibilityPageObjects::AccessibilityPageObjects() = default;

AccessibilityPageObjects::AccessibilityPageObjects(
    const std::vector<AccessibilityLinkInfo>& links,
    const std::vector<AccessibilityImageInfo>& images,
    const std::vector<AccessibilityHighlightInfo>& highlights)
    : links(links), images(images), highlights(highlights) {}

AccessibilityPageObjects::AccessibilityPageObjects(
    const AccessibilityPageObjects& other) = default;

AccessibilityPageObjects::~AccessibilityPageObjects() = default;

AccessibilityViewportInfo::AccessibilityViewportInfo() = default;
AccessibilityViewportInfo::AccessibilityViewportInfo(
    const AccessibilityViewportInfo& other) = default;
AccessibilityViewportInfo::~AccessibilityViewportInfo() = default;

AccessibilityActionData::AccessibilityActionData() = default;

AccessibilityActionData::AccessibilityActionData(
    AccessibilityAction action,
    AccessibilityAnnotationType annotation_type,
    const gfx::Point& target_point,
    const gfx::Rect& target_rect,
    uint32_t annotation_index,
    uint32_t page_index,
    AccessibilityScrollAlignment horizontal_scroll_alignment,
    AccessibilityScrollAlignment vertical_scroll_alignment,
    const PageCharacterIndex& selection_start_index,
    const PageCharacterIndex& selection_end_index)
    : action(action),
      annotation_type(annotation_type),
      target_point(target_point),
      target_rect(target_rect),
      annotation_index(annotation_index),
      page_index(page_index),
      horizontal_scroll_alignment(horizontal_scroll_alignment),
      vertical_scroll_alignment(vertical_scroll_alignment),
      selection_start_index(selection_start_index),
      selection_end_index(selection_end_index) {}

AccessibilityActionData::AccessibilityActionData(
    const AccessibilityActionData& other) = default;

AccessibilityActionData::~AccessibilityActionData() = default;

}  // namespace chrome_pdf
