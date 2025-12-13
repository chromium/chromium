// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/context_menu_params_builder.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

namespace blink {

namespace {

blink::mojom::CustomContextMenuItemPtr MenuItemBuild(
    const blink::MenuItemInfo& item) {
  auto result = blink::mojom::CustomContextMenuItem::New();
  if (item.accelerator.has_value()) {
    auto accelerator = blink::mojom::Accelerator::New();
    accelerator->key_code = item.accelerator->key_code;
    accelerator->modifiers = item.accelerator->modifiers;
    result->accelerator = std::move(accelerator);
  }
  result->label = item.label;
  result->feature_name = item.feature_name;
  result->tool_tip = item.tool_tip;
  result->type =
      static_cast<blink::mojom::CustomContextMenuItemType>(item.type);
  result->action = item.action;
  result->is_experimental_feature = item.is_experimental_feature;
  result->rtl = (item.text_direction == base::i18n::RIGHT_TO_LEFT);
  result->has_directional_override = item.has_text_direction_override;
  result->enabled = item.enabled;
  result->checked = item.checked;
  result->force_show_accelerator_for_item =
      item.force_show_accelerator_for_item;
  for (const auto& sub_menu_item : item.sub_menu_items)
    result->submenu.push_back(MenuItemBuild(sub_menu_item));

  return result;
}

}  // namespace

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
  params.is_image_media_plugin_document = data.is_image_media_plugin_document;
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
  params.annotation_type = data.annotation_type;
  params.opened_from_interest_for = data.opened_from_interest_for;
  params.interest_for_node_id = data.interest_for_node_id;

  for (const auto& suggestion : data.dictionary_suggestions)
    params.dictionary_suggestions.push_back(suggestion);

  for (const auto& item : data.custom_items)
    params.custom_items.push_back(MenuItemBuild(item));

  params.link_text = base::UTF8ToUTF16(data.link_text);

  if (data.impression)
    params.impression = data.impression;

  params.form_control_type = data.form_control_type;
  params.is_content_editable_for_autofill =
      data.is_content_editable_for_autofill;
  params.field_renderer_id = data.field_renderer_id;
  params.form_renderer_id = data.form_renderer_id;

  // TODO(crbug.com/373340199): Remove `WebMenuSourceType` and static_cast
  params.source_type = static_cast<ui::mojom::MenuSourceType>(data.source_type);

  return params;
}

}  // namespace blink
