// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/context_menu_mojom_traits.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::FormRendererIdDataView, uint64_t>::Read(
    blink::mojom::FormRendererIdDataView data,
    uint64_t* out) {
  *out = data.id();
  return true;
}

// static
bool StructTraits<blink::mojom::FieldRendererIdDataView, uint64_t>::Read(
    blink::mojom::FieldRendererIdDataView data,
    uint64_t* out) {
  *out = data.id();
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
      !data.ReadSelectionRect(&out->selection_rect) ||
      !data.ReadFormControlType(&out->form_control_type) ||
      !data.ReadFormRendererId(&out->form_renderer_id) ||
      !data.ReadFieldRendererId(&out->field_renderer_id)) {
    return false;
  }

  out->x = data.x();
  out->y = data.y();
  out->has_image_contents = data.has_image_contents();
  out->is_image_media_plugin_document = data.is_image_media_plugin_document();
  out->media_flags = data.media_flags();
  out->spellcheck_enabled = data.spellcheck_enabled();
  out->is_editable = data.is_editable();
  out->writing_direction_default = data.writing_direction_default();
  out->writing_direction_left_to_right = data.writing_direction_left_to_right();
  out->writing_direction_right_to_left = data.writing_direction_right_to_left();
  out->edit_flags = data.edit_flags();
  out->selection_start_offset = data.selection_start_offset();
  out->opened_from_highlight = data.opened_from_highlight();
  out->is_content_editable_for_autofill =
      data.is_content_editable_for_autofill();
  out->is_password_type_by_heuristics = data.is_password_type_by_heuristics();
  return true;
}

}  // namespace mojo
