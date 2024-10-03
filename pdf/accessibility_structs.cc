// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility_structs.h"

namespace chrome_pdf {

bool AccessibilityDocInfo::operator==(const AccessibilityDocInfo& other) const {
  return page_count == other.page_count &&
         text_accessible == other.text_accessible &&
         text_copyable == other.text_copyable;
}

bool AccessibilityDocInfo::operator!=(const AccessibilityDocInfo& other) const {
  return !(*this == other);
}

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo() = default;

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const std::string& font_name,
    int font_weight,
    AccessibilityTextRenderMode render_mode,
    float font_size,
    uint32_t fill_color,
    uint32_t stroke_color,
    bool is_italic,
    bool is_bold)
    : font_name(font_name),
      font_weight(font_weight),
      render_mode(render_mode),
      font_size(font_size),
      fill_color(fill_color),
      stroke_color(stroke_color),
      is_italic(is_italic),
      is_bold(is_bold) {}

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const AccessibilityTextStyleInfo& other) = default;

AccessibilityTextStyleInfo::~AccessibilityTextStyleInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    uint32_t len,
    const gfx::RectF& bounds,
    AccessibilityTextDirection direction,
    const AccessibilityTextStyleInfo& style)
    : AccessibilityTextRunInfo(len,
                               bounds,
                               direction,
                               style,
                               /*is_searchified=*/false) {}

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    uint32_t len,
    const gfx::RectF& bounds,
    AccessibilityTextDirection direction,
    const AccessibilityTextStyleInfo& style,
    bool is_searchified)
    : len(len),
      bounds(bounds),
      direction(direction),
      style(style),
      is_searchified(is_searchified) {}

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    const AccessibilityTextRunInfo& other) = default;

AccessibilityTextRunInfo::~AccessibilityTextRunInfo() = default;

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

AccessibilityTextFieldInfo::AccessibilityTextFieldInfo() = default;

AccessibilityTextFieldInfo::AccessibilityTextFieldInfo(const std::string& name,
                                                       const std::string& value,
                                                       bool is_read_only,
                                                       bool is_required,
                                                       bool is_password,
                                                       uint32_t index_in_page,
                                                       uint32_t text_run_index,
                                                       const gfx::RectF& bounds)
    : name(name),
      value(value),
      is_read_only(is_read_only),
      is_required(is_required),
      is_password(is_password),
      index_in_page(index_in_page),
      text_run_index(text_run_index),
      bounds(bounds) {}

AccessibilityTextFieldInfo::AccessibilityTextFieldInfo(
    const AccessibilityTextFieldInfo& other) = default;

AccessibilityTextFieldInfo::~AccessibilityTextFieldInfo() = default;

AccessibilityChoiceFieldInfo::AccessibilityChoiceFieldInfo() = default;

AccessibilityChoiceFieldInfo::AccessibilityChoiceFieldInfo(
    const std::string& name,
    const std::vector<AccessibilityChoiceFieldOptionInfo>& options,
    ChoiceFieldType type,
    bool is_read_only,
    bool is_multi_select,
    bool has_editable_text_box,
    uint32_t index_in_page,
    uint32_t text_run_index,
    const gfx::RectF& bounds)
    : name(name),
      options(options),
      type(type),
      is_read_only(is_read_only),
      is_multi_select(is_multi_select),
      has_editable_text_box(has_editable_text_box),
      index_in_page(index_in_page),
      text_run_index(text_run_index),
      bounds(bounds) {}

AccessibilityChoiceFieldInfo::AccessibilityChoiceFieldInfo(
    const AccessibilityChoiceFieldInfo& other) = default;

AccessibilityChoiceFieldInfo::~AccessibilityChoiceFieldInfo() = default;

AccessibilityButtonInfo::AccessibilityButtonInfo() = default;

AccessibilityButtonInfo::AccessibilityButtonInfo(const std::string& name,
                                                 const std::string& value,
                                                 ButtonType type,
                                                 bool is_read_only,
                                                 bool is_checked,
                                                 uint32_t control_count,
                                                 uint32_t control_index,
                                                 uint32_t index_in_page,
                                                 uint32_t text_run_index,
                                                 const gfx::RectF& bounds)
    : name(name),
      value(value),
      type(type),
      is_read_only(is_read_only),
      is_checked(is_checked),
      control_count(control_count),
      control_index(control_index),
      index_in_page(index_in_page),
      text_run_index(text_run_index),
      bounds(bounds) {}

AccessibilityButtonInfo::AccessibilityButtonInfo(
    const AccessibilityButtonInfo& other) = default;

AccessibilityButtonInfo::~AccessibilityButtonInfo() = default;

AccessibilityFormFieldInfo::AccessibilityFormFieldInfo() = default;

AccessibilityFormFieldInfo::AccessibilityFormFieldInfo(
    const std::vector<AccessibilityTextFieldInfo>& text_fields,
    const std::vector<AccessibilityChoiceFieldInfo>& choice_fields,
    const std::vector<AccessibilityButtonInfo>& buttons)
    : text_fields(text_fields),
      choice_fields(choice_fields),
      buttons(buttons) {}

AccessibilityFormFieldInfo::AccessibilityFormFieldInfo(
    const AccessibilityFormFieldInfo& other) = default;

AccessibilityFormFieldInfo::~AccessibilityFormFieldInfo() = default;

AccessibilityPageObjects::AccessibilityPageObjects() = default;

AccessibilityPageObjects::AccessibilityPageObjects(
    const std::vector<AccessibilityLinkInfo>& links,
    const std::vector<AccessibilityImageInfo>& images,
    const std::vector<AccessibilityHighlightInfo>& highlights,
    const AccessibilityFormFieldInfo& form_fields)
    : links(links),
      images(images),
      highlights(highlights),
      form_fields(form_fields) {}

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
