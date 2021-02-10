// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/context_menu_mojom_traits.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/context_menu_data/menu_item.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"

namespace mojo {

// static
blink::mojom::CustomContextMenuItemType
EnumTraits<blink::mojom::CustomContextMenuItemType,
           blink::MenuItem::Type>::ToMojom(blink::MenuItem::Type input) {
  switch (input) {
    case blink::MenuItem::Type::OPTION:
      return blink::mojom::CustomContextMenuItemType::kOption;
    case blink::MenuItem::Type::CHECKABLE_OPTION:
      return blink::mojom::CustomContextMenuItemType::kCheckableOption;
    case blink::MenuItem::Type::GROUP:
      return blink::mojom::CustomContextMenuItemType::kGroup;
    case blink::MenuItem::Type::SEPARATOR:
      return blink::mojom::CustomContextMenuItemType::kSeparator;
    case blink::MenuItem::Type::SUBMENU:
      return blink::mojom::CustomContextMenuItemType::kSubMenu;
  }

  NOTREACHED();
  return blink::mojom::CustomContextMenuItemType::kMaxValue;
}

// static
bool EnumTraits<blink::mojom::CustomContextMenuItemType,
                blink::MenuItem::Type>::
    FromMojom(blink::mojom::CustomContextMenuItemType input,
              blink::MenuItem::Type* output) {
  switch (input) {
    case blink::mojom::CustomContextMenuItemType::kOption:
      *output = blink::MenuItem::Type::OPTION;
      return true;
    case blink::mojom::CustomContextMenuItemType::kCheckableOption:
      *output = blink::MenuItem::Type::CHECKABLE_OPTION;
      return true;
    case blink::mojom::CustomContextMenuItemType::kGroup:
      *output = blink::MenuItem::Type::GROUP;
      return true;
    case blink::mojom::CustomContextMenuItemType::kSeparator:
      *output = blink::MenuItem::Type::SEPARATOR;
      return true;
    case blink::mojom::CustomContextMenuItemType::kSubMenu:
      *output = blink::MenuItem::Type::SUBMENU;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<blink::mojom::ImpressionDataView, blink::Impression>::Read(
    blink::mojom::ImpressionDataView data,
    blink::Impression* out) {
  if (!data.ReadConversionDestination(&out->conversion_destination) ||
      !data.ReadReportingOrigin(&out->reporting_origin) ||
      !data.ReadExpiry(&out->expiry))
    return false;

  out->impression_data = data.impression_data();
  return true;
}

// static
bool StructTraits<
    blink::mojom::CustomContextMenuItemDataView,
    blink::MenuItem>::Read(blink::mojom::CustomContextMenuItemDataView data,
                           blink::MenuItem* out) {
  if (!data.ReadLabel(&out->label) || !data.ReadIcon(&out->icon) ||
      !data.ReadToolTip(&out->tool_tip) || !data.ReadType(&out->type) ||
      !data.ReadSubmenu(&out->submenu))
    return false;

  out->action = data.action();
  out->rtl = data.rtl();
  out->has_directional_override = data.has_directional_override();
  out->enabled = data.enabled();
  out->checked = data.checked();
  return true;
}

// static
bool StructTraits<blink::mojom::UntrustworthyContextMenuParamsDataView,
                  blink::UntrustworthyContextMenuParams>::
    Read(blink::mojom::UntrustworthyContextMenuParamsDataView data,
         blink::UntrustworthyContextMenuParams* out) {
  if (!data.ReadMediaType(&out->media_type) ||
      !data.ReadLinkUrl(&out->link_url) ||
      !data.ReadLinkText(&out->link_text) ||
      !data.ReadImpression(&out->impression) ||
      !data.ReadUnfilteredLinkUrl(&out->unfiltered_link_url) ||
      !data.ReadSrcUrl(&out->src_url) ||
      !data.ReadSelectionText(&out->selection_text) ||
      !data.ReadTitleText(&out->title_text) ||
      !data.ReadAltText(&out->alt_text) ||
      !data.ReadSuggestedFilename(&out->suggested_filename) ||
      !data.ReadMisspelledWord(&out->misspelled_word) ||
      !data.ReadDictionarySuggestions(&out->dictionary_suggestions) ||
      !data.ReadFrameCharset(&out->frame_charset) ||
      !data.ReadReferrerPolicy(&out->referrer_policy) ||
      !data.ReadLinkFollowed(&out->link_followed) ||
      !data.ReadCustomItems(&out->custom_items) ||
      !data.ReadSourceType(&out->source_type) ||
      !data.ReadInputFieldType(&out->input_field_type) ||
      !data.ReadSelectionRect(&out->selection_rect))
    return false;

  out->x = data.x();
  out->y = data.y();
  out->has_image_contents = data.has_image_contents();
  out->media_flags = data.media_flags();
  out->spellcheck_enabled = data.spellcheck_enabled();
  out->is_editable = data.is_editable();
  out->writing_direction_default = data.writing_direction_default();
  out->writing_direction_left_to_right = data.writing_direction_left_to_right();
  out->writing_direction_right_to_left = data.writing_direction_right_to_left();
  out->edit_flags = data.edit_flags();
  out->selection_start_offset = data.selection_start_offset();
  return true;
}

}  // namespace mojo
