// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"

#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

namespace blink {

UntrustworthyContextMenuParams::UntrustworthyContextMenuParams()
    : media_type(blink::mojom::ContextMenuDataMediaType::kNone),
      x(0),
      y(0),
      has_image_contents(false),
      is_image_media_plugin_document(false),
      media_flags(0),
      spellcheck_enabled(false),
      is_editable(false),
      writing_direction_default(
          blink::ContextMenuData::kCheckableMenuItemDisabled),
      writing_direction_left_to_right(
          blink::ContextMenuData::kCheckableMenuItemEnabled),
      writing_direction_right_to_left(
          blink::ContextMenuData::kCheckableMenuItemEnabled),
      edit_flags(0),
      referrer_policy(network::mojom::ReferrerPolicy::kDefault),
      source_type(ui::MENU_SOURCE_NONE),
      selection_start_offset(0) {}

UntrustworthyContextMenuParams::UntrustworthyContextMenuParams(
    const UntrustworthyContextMenuParams& other) {
  Assign(other);
}

UntrustworthyContextMenuParams& UntrustworthyContextMenuParams::operator=(
    const UntrustworthyContextMenuParams& other) {
  if (&other == this)
    return *this;
  Assign(other);
  return *this;
}

void UntrustworthyContextMenuParams::Assign(
    const UntrustworthyContextMenuParams& other) {
  media_type = other.media_type;
  x = other.x;
  y = other.y;
  link_url = other.link_url;
  link_text = other.link_text;
  impression = other.impression;
  unfiltered_link_url = other.unfiltered_link_url;
  src_url = other.src_url;
  has_image_contents = other.has_image_contents;
  is_image_media_plugin_document = other.is_image_media_plugin_document;
  media_flags = other.media_flags;
  selection_text = other.selection_text;
  title_text = other.title_text;
  alt_text = other.alt_text;
  suggested_filename = other.suggested_filename;
  misspelled_word = other.misspelled_word;
  dictionary_suggestions = other.dictionary_suggestions;
  spellcheck_enabled = other.spellcheck_enabled;
  is_editable = other.is_editable;
  writing_direction_default = other.writing_direction_default;
  writing_direction_left_to_right = other.writing_direction_left_to_right;
  writing_direction_right_to_left = other.writing_direction_right_to_left;
  edit_flags = other.edit_flags;
  frame_charset = other.frame_charset;
  referrer_policy = other.referrer_policy;
  link_followed = other.link_followed;
  for (auto& item : other.custom_items)
    custom_items.push_back(item.Clone());
  source_type = other.source_type;
  selection_rect = other.selection_rect;
  selection_start_offset = other.selection_start_offset;
  opened_from_highlight = other.opened_from_highlight;
  form_control_type = other.form_control_type;
  is_content_editable_for_autofill = other.is_content_editable_for_autofill;
  field_renderer_id = other.field_renderer_id;
  form_renderer_id = other.form_renderer_id;
  is_password_type_by_heuristics = other.is_password_type_by_heuristics;
}

UntrustworthyContextMenuParams::~UntrustworthyContextMenuParams() = default;

}  // namespace blink
