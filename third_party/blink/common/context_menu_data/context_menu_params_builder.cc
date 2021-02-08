// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/context_menu_params_builder.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/menu_item.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"

namespace blink {

// static
UntrustworthyContextMenuParams ContextMenuParamsBuilder::Build(
    const blink::ContextMenuData& data) {
  blink::UntrustworthyContextMenuParams params;
  params.media_type = data.media_type;
  params.x = data.mouse_position.x();
  params.y = data.mouse_position.y();
  params.link_url = data.link_url;
  params.unfiltered_link_url = data.link_url;
  params.src_url = data.src_url;
  params.has_image_contents = data.has_image_contents;
  params.media_flags = data.media_flags;
  params.selection_text = base::UTF8ToUTF16(data.selected_text);
  params.selection_start_offset = data.selection_start_offset;
  params.title_text = base::UTF8ToUTF16(data.title_text);
  params.alt_text = base::UTF8ToUTF16(data.alt_text);
  params.misspelled_word = data.misspelled_word;
  params.spellcheck_enabled = data.is_spell_checking_enabled;
  params.is_editable = data.is_editable;
  params.writing_direction_default = data.writing_direction_default;
  params.writing_direction_left_to_right = data.writing_direction_left_to_right;
  params.writing_direction_right_to_left = data.writing_direction_right_to_left;
  params.edit_flags = data.edit_flags;
  params.frame_charset = data.frame_encoding;
  params.referrer_policy = data.referrer_policy;
  params.suggested_filename = base::UTF8ToUTF16(data.suggested_filename);
  params.input_field_type = data.input_field_type;

  for (size_t i = 0; i < data.dictionary_suggestions.size(); ++i)
    params.dictionary_suggestions.push_back(data.dictionary_suggestions[i]);

  for (size_t i = 0; i < data.custom_items.size(); ++i) {
    params.custom_items.push_back(
        blink::MenuItemBuilder::Build(data.custom_items[i]));
  }

  params.link_text = base::UTF8ToUTF16(data.link_text);

  if (data.impression)
    params.impression = data.impression;

  params.source_type = static_cast<ui::MenuSourceType>(data.source_type);

  return params;
}

}  // namespace blink
