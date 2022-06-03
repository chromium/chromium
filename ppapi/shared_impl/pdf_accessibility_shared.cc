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

PdfAccessibilityHighlightInfo::PdfAccessibilityHighlightInfo() = default;

PdfAccessibilityHighlightInfo::~PdfAccessibilityHighlightInfo() = default;

PdfAccessibilityHighlightInfo::PdfAccessibilityHighlightInfo(
    const PP_PrivateAccessibilityHighlightInfo& highlight)
    : note_text(std::string(highlight.note_text, highlight.note_text_length)),
      index_in_page(highlight.index_in_page),
      text_run_index(highlight.text_run_index),
      text_run_count(highlight.text_run_count),
      bounds(highlight.bounds),
      color(highlight.color) {}

PdfAccessibilityTextFieldInfo::PdfAccessibilityTextFieldInfo() = default;

PdfAccessibilityTextFieldInfo::~PdfAccessibilityTextFieldInfo() = default;

PdfAccessibilityTextFieldInfo::PdfAccessibilityTextFieldInfo(
    const PP_PrivateAccessibilityTextFieldInfo& text_field)
    : name(std::string(text_field.name, text_field.name_length)),
      value(std::string(text_field.value, text_field.value_length)),
      is_read_only(text_field.is_read_only),
      is_required(text_field.is_required),
      is_password(text_field.is_password),
      index_in_page(text_field.index_in_page),
      text_run_index(text_field.text_run_index),
      bounds(text_field.bounds) {}

PdfAccessibilityChoiceFieldOptionInfo::PdfAccessibilityChoiceFieldOptionInfo() =
    default;

PdfAccessibilityChoiceFieldOptionInfo::
    ~PdfAccessibilityChoiceFieldOptionInfo() = default;

PdfAccessibilityChoiceFieldOptionInfo::PdfAccessibilityChoiceFieldOptionInfo(
    const PP_PrivateAccessibilityChoiceFieldOptionInfo& option)
    : name(std::string(option.name, option.name_length)),
      is_selected(option.is_selected),
      bounds(option.bounds) {}

PdfAccessibilityChoiceFieldInfo::PdfAccessibilityChoiceFieldInfo() = default;

PdfAccessibilityChoiceFieldInfo::~PdfAccessibilityChoiceFieldInfo() = default;

PdfAccessibilityChoiceFieldInfo::PdfAccessibilityChoiceFieldInfo(
    const PP_PrivateAccessibilityChoiceFieldInfo& choice_field)
    : name(std::string(choice_field.name, choice_field.name_length)),
      type(choice_field.type),
      is_read_only(choice_field.is_read_only),
      is_multi_select(choice_field.is_multi_select),
      has_editable_text_box(choice_field.has_editable_text_box),
      index_in_page(choice_field.index_in_page),
      text_run_index(choice_field.text_run_index),
      bounds(choice_field.bounds) {
  options.reserve(choice_field.options_length);
  for (size_t i = 0; i < choice_field.options_length; i++) {
    options.emplace_back(choice_field.options[i]);
  }
}

PdfAccessibilityButtonInfo::PdfAccessibilityButtonInfo() = default;

PdfAccessibilityButtonInfo::~PdfAccessibilityButtonInfo() = default;

PdfAccessibilityButtonInfo::PdfAccessibilityButtonInfo(
    const PP_PrivateAccessibilityButtonInfo& button)
    : name(std::string(button.name, button.name_length)),
      value(std::string(button.value, button.value_length)),
      type(button.type),
      is_read_only(button.is_read_only),
      is_checked(button.is_checked),
      control_count(button.control_count),
      control_index(button.control_index),
      index_in_page(button.index_in_page),
      text_run_index(button.text_run_index),
      bounds(button.bounds) {}

PdfAccessibilityFormFieldInfo::PdfAccessibilityFormFieldInfo() = default;

PdfAccessibilityFormFieldInfo::PdfAccessibilityFormFieldInfo(
    const PP_PrivateAccessibilityFormFieldInfo& form_fields) {
  text_fields.reserve(form_fields.text_field_count);
  for (size_t i = 0; i < form_fields.text_field_count; i++) {
    text_fields.emplace_back(form_fields.text_fields[i]);
  }

  choice_fields.reserve(form_fields.choice_field_count);
  for (size_t i = 0; i < form_fields.choice_field_count; i++) {
    choice_fields.emplace_back(form_fields.choice_fields[i]);
  }

  buttons.reserve(form_fields.button_count);
  for (size_t i = 0; i < form_fields.button_count; i++) {
    buttons.emplace_back(form_fields.buttons[i]);
  }
}

PdfAccessibilityFormFieldInfo::~PdfAccessibilityFormFieldInfo() = default;

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects() = default;

PdfAccessibilityPageObjects::PdfAccessibilityPageObjects(
    const PP_PrivateAccessibilityPageObjects& page_objects)
    : form_fields(page_objects.form_fields) {
  links.reserve(page_objects.link_count);
  for (size_t i = 0; i < page_objects.link_count; i++) {
    links.emplace_back(page_objects.links[i]);
  }

  images.reserve(page_objects.image_count);
  for (size_t i = 0; i < page_objects.image_count; i++) {
    images.emplace_back(page_objects.images[i]);
  }

  highlights.reserve(page_objects.highlight_count);
  for (size_t i = 0; i < page_objects.highlight_count; i++) {
    highlights.emplace_back(page_objects.highlights[i]);
  }
}

PdfAccessibilityPageObjects::~PdfAccessibilityPageObjects() = default;

}  // namespace ppapi
