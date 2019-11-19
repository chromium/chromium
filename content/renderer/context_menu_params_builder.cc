// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/context_menu_params_builder.h"

#include <stddef.h>

#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/menu_item_builder.h"

namespace content {

// static
ContextMenuParams ContextMenuParamsBuilder::Build(
    const blink::WebContextMenuData& data) {
  ContextMenuParams params;
  params.media_type = data.media_type;
  params.x = data.mouse_position.x;
  params.y = data.mouse_position.y;
  params.link_url = data.link_url;
  params.unfiltered_link_url = data.link_url;
  params.src_url = data.src_url;
  params.has_image_contents = data.has_image_contents;
  params.page_url = data.page_url;
  params.frame_url = data.frame_url;
  params.media_flags = data.media_flags;
  params.selection_text = data.selected_text.Utf16();
  params.selection_start_offset = data.selection_start_offset;
  params.title_text = data.title_text.Utf16();
  params.alt_text = data.alt_text.Utf16();
  params.misspelled_word = data.misspelled_word.Utf16();
  params.spellcheck_enabled = data.is_spell_checking_enabled;
  params.is_editable = data.is_editable;
  params.writing_direction_default = data.writing_direction_default;
  params.writing_direction_left_to_right = data.writing_direction_left_to_right;
  params.writing_direction_right_to_left = data.writing_direction_right_to_left;
  params.edit_flags = data.edit_flags;
  params.frame_charset = data.frame_encoding.Utf8();
  params.referrer_policy = data.referrer_policy;
  params.suggested_filename = data.suggested_filename.Utf16();
  params.input_field_type = data.input_field_type;

  for (size_t i = 0; i < data.dictionary_suggestions.size(); ++i)
    params.dictionary_suggestions.push_back(
        data.dictionary_suggestions[i].Utf16());

  params.custom_context.is_pepper_menu = false;
  for (size_t i = 0; i < data.custom_items.size(); ++i)
    params.custom_items.push_back(MenuItemBuilder::Build(data.custom_items[i]));

  params.link_text = data.link_text.Utf16();
  params.source_type = static_cast<ui::MenuSourceType>(data.source_type);

  return params;
}

}  // namespace content
