// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_UNTRUSTWORTHY_CONTEXT_MENU_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_UNTRUSTWORTHY_CONTEXT_MENU_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace blink {

// SECURITY NOTE: Data in this struct is untrustworthy, because it is sent in a
// Mojo from a renderer process.  The browser process should use
// ContextMenuParams, after validating UntrustworthyContextMenuParams in a Mojo
// handling routine.
struct BLINK_COMMON_EXPORT UntrustworthyContextMenuParams {
  UntrustworthyContextMenuParams();
  UntrustworthyContextMenuParams(const UntrustworthyContextMenuParams& other);
  UntrustworthyContextMenuParams& operator=(
      const UntrustworthyContextMenuParams& other);
  ~UntrustworthyContextMenuParams();

  // This is the type of Context Node that the context menu was invoked on.
  blink::mojom::ContextMenuDataMediaType media_type;

  // These values represent the coordinates of the mouse when the context menu
  // was invoked.  Coords are relative to the associated RenderView's origin.
  int x;
  int y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // The text associated with the link. May be an empty string if the contents
  // of the link are an image.
  // Will be empty if |link_url| is empty.
  std::u16string link_text;

  // The impression declared by the link. May be absl::nullopt even if
  // |link_url| is non-empty.
  absl::optional<blink::Impression> impression;

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  GURL unfiltered_link_url;

  // This is the source URL for the element that the context menu was
  // invoked on. Example of elements with source URLs are img, audio, and
  // video.
  GURL src_url;

  // This is true if the context menu was invoked on an image which has
  // non-empty contents.
  bool has_image_contents;

  // These are the parameters for the media element that the context menu
  // was invoked on.
  int media_flags;

  // This is the text of the selection that the context menu was invoked on.
  std::u16string selection_text;

  // This is the title text of the selection that the context menu was
  // invoked on.
  std::u16string title_text;

  // This is the alt text of the selection that the context menu was
  // invoked on.
  std::u16string alt_text;

  // This is the suggested filename to be used when saving file through "Save
  // Link As" option of context menu.
  std::u16string suggested_filename;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  std::u16string misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<std::u16string> dictionary_suggestions;

  // If editable, flag for whether spell check is enabled or not.
  bool spellcheck_enabled;

  // Whether context is editable.
  bool is_editable;

  // Writing direction menu items.
  int writing_direction_default;
  int writing_direction_left_to_right;
  int writing_direction_right_to_left;

  // These flags indicate to the browser whether the renderer believes it is
  // able to perform the corresponding action.
  int edit_flags;

  // The character encoding of the frame on which the menu is invoked.
  std::string frame_charset;

  // The referrer policy of the frame on which the menu is invoked.
  network::mojom::ReferrerPolicy referrer_policy;

  GURL link_followed;
  std::vector<blink::mojom::CustomContextMenuItemPtr> custom_items;

  ui::MenuSourceType source_type;

  // If this node is an input field, the type of that field.
  blink::mojom::ContextMenuDataInputFieldType input_field_type;

  // True iff a field's type is plain text but heuristics (e.g. the name
  // attribute contains 'password' as a substring) recognize it as a password
  // field.
  bool is_password_type_by_heuristics = false;

  // For the outermost main frame's widget, this will be the selection rect in
  // viewport space. For a local root, this is in the coordinates of the local
  // frame root.
  gfx::Rect selection_rect;

  // Start position of the selection text.
  int selection_start_offset;

  // The context menu was opened by right clicking on an existing
  // highlight/fragment.
  bool opened_from_highlight = false;

  // The context menu was opened on an input or textarea field. Denotes the
  // renderer id of the form containing the input or the textarea field.
  // `absl::nullopt` if the click was not on an input field or a formless field.
  absl::optional<uint64_t> form_renderer_id;

  // The context menu was opened on an input or textarea field.
  // Otherwise, `absl::nullopt`.
  absl::optional<uint64_t> field_renderer_id;

 private:
  void Assign(const UntrustworthyContextMenuParams& other);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_UNTRUSTWORTHY_CONTEXT_MENU_PARAMS_H_
