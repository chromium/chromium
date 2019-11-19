/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_CONTEXT_MENU_DATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_CONTEXT_MENU_DATA_H_

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/context_menu_data/input_field_type.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"
#include "third_party/blink/public/platform/web_menu_source_type.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_menu_item_info.h"

namespace blink {

// This struct is passed to WebViewClient::ShowContextMenu.
struct WebContextMenuData {
  // The type of media the context menu is being invoked on.
  // using MediaType = ContextMenuDataMediaType;
  ContextMenuDataMediaType media_type;

  // The x and y position of the mouse pointer (relative to the webview).
  WebPoint mouse_position;

  // The absolute URL of the link that is in context.
  WebURL link_url;

  // The absolute URL of the image/video/audio that is in context.
  WebURL src_url;

  // Whether the image in context is a null.
  bool has_image_contents;

  // The absolute URL of the page in context.
  WebURL page_url;

  // The absolute URL of the subframe in context.
  WebURL frame_url;

  // The encoding for the frame in context.
  WebString frame_encoding;

  enum MediaFlags {
    kMediaNone = 0x0,
    kMediaInError = 0x1,
    kMediaPaused = 0x2,
    kMediaMuted = 0x4,
    kMediaLoop = 0x8,
    kMediaCanSave = 0x10,
    kMediaHasAudio = 0x20,
    kMediaCanToggleControls = 0x40,
    kMediaControls = 0x80,
    kMediaCanPrint = 0x100,
    kMediaCanRotate = 0x200,
    kMediaCanPictureInPicture = 0x400,
    kMediaPictureInPicture = 0x800,
    kMediaCanLoop = 0x1000,
  };

  // Extra attributes describing media elements.
  int media_flags;

  // The text of the link that is in the context.
  WebString link_text;

  // The raw text of the selection in context.
  WebString selected_text;

  // Title attribute of the selection in context.
  WebString title_text;

  // Alt attribute of the selection in context.
  WebString alt_text;

  // Whether spell checking is enabled.
  bool is_spell_checking_enabled;

  // Suggested filename for saving file.
  WebString suggested_filename;

  // The editable (possibily) misspelled word.
  WebString misspelled_word;

  // If misspelledWord is not empty, holds suggestions from the dictionary.
  WebVector<WebString> dictionary_suggestions;

  // Whether context is editable.
  bool is_editable;

  // If this node is an input field, the type of that field.
  ContextMenuDataInputFieldType input_field_type;

  enum CheckableMenuItemFlags {
    kCheckableMenuItemDisabled = 0x0,
    kCheckableMenuItemEnabled = 0x1,
    kCheckableMenuItemChecked = 0x2,
  };

  // Writing direction menu items - values are unions of
  // CheckableMenuItemFlags.
  int writing_direction_default;
  int writing_direction_left_to_right;
  int writing_direction_right_to_left;

  // Which edit operations are available in the context.
  int edit_flags;

  // The referrer policy applicable to this context.
  network::mojom::ReferrerPolicy referrer_policy;

  // Custom context menu items provided by the WebCore internals.
  WebVector<WebMenuItemInfo> custom_items;

  // Selection in viewport coordinates.
  WebRect selection_rect;

  // TODO(https://crbug.com/781914): Remove this field after we done with Blink
  // side tracking.
  // Global index of start position for the current selection.
  // If the current element is editable, then it starts from the first
  // character within the element, otherwise, it starts from the beginning of
  // the current webpage.
  int selection_start_offset;

  WebMenuSourceType source_type;

  WebContextMenuData()
      : media_type(ContextMenuDataMediaType::kNone),
        has_image_contents(false),
        media_flags(kMediaNone),
        is_spell_checking_enabled(false),
        is_editable(false),
        writing_direction_default(kCheckableMenuItemDisabled),
        writing_direction_left_to_right(kCheckableMenuItemEnabled),
        writing_direction_right_to_left(kCheckableMenuItemEnabled),
        edit_flags(0) {}
};

}  // namespace blink

#endif
