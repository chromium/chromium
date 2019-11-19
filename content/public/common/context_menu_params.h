// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/menu_item.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT CustomContextMenuContext {
  static const int32_t kCurrentRenderWidget;

  CustomContextMenuContext();

  bool is_pepper_menu;
  int request_id;
  // The routing ID of the render widget on which the context menu is shown.
  // It could also be |kCurrentRenderWidget|, which means the render widget that
  // the corresponding ViewHostMsg_ContextMenu is sent to.
  int32_t render_widget_id;

  // If the context menu was created for a link, and we navigated to that url,
  // this will contain the url that was navigated. This field may not be set
  // if, for example, we are transitioning to an incognito window, since we
  // want to sever any connection to the old renderer.
  GURL link_followed;
};

// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
struct CONTENT_EXPORT ContextMenuParams {
  ContextMenuParams();
  ContextMenuParams(const ContextMenuParams& other);
  ~ContextMenuParams();

  // This is the type of Context Node that the context menu was invoked on.
  blink::ContextMenuDataMediaType media_type;

  // These values represent the coordinates of the mouse when the context menu
  // was invoked.  Coords are relative to the associated RenderView's origin.
  int x;
  int y;

  // This is the URL of the link that encloses the node the context menu was
  // invoked on.
  GURL link_url;

  // The text associated with the link. May be an empty string if the contents
  // of the link are an image.
  // Will be empty if link_url is empty.
  base::string16 link_text;

  // The link URL to be used ONLY for "copy link address". We don't validate
  // this field in the frontend process.
  GURL unfiltered_link_url;

  // This is the source URL for the element that the context menu was
  // invoked on.  Example of elements with source URLs are img, audio, and
  // video.
  GURL src_url;

  // This is true if the context menu was invoked on an image which has
  // non-empty contents.
  bool has_image_contents;

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the URL of the subframe that the context menu was invoked on.
  GURL frame_url;

  // These are the parameters for the media element that the context menu
  // was invoked on.
  int media_flags;

  // This is the text of the selection that the context menu was invoked on.
  base::string16 selection_text;

  // This is the title text of the selection that the context menu was
  // invoked on.
  base::string16 title_text;

  // This is the alt text of the selection that the context menu was
  // invoked on.
  base::string16 alt_text;

  // This is the suggested filename to be used when saving file through "Save
  // Link As" option of context menu.
  base::string16 suggested_filename;

  // The misspelled word under the cursor, if any. Used to generate the
  // |dictionary_suggestions| list.
  base::string16 misspelled_word;

  // Suggested replacements for a misspelled word under the cursor.
  // This vector gets populated in the render process host
  // by intercepting ViewHostMsg_ContextMenu in ResourceMessageFilter
  // and populating dictionary_suggestions if the type is EDITABLE
  // and the misspelled_word is not empty.
  std::vector<base::string16> dictionary_suggestions;

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

  CustomContextMenuContext custom_context;
  std::vector<MenuItem> custom_items;

  ui::MenuSourceType source_type;

  // Extra properties for the context menu.
  std::map<std::string, std::string> properties;

  // If this node is an input field, the type of that field.
  blink::ContextMenuDataInputFieldType input_field_type;

  // Rect representing the coordinates in the document space of the selection.
  gfx::Rect selection_rect;

  // Start position of the selection text.
  int selection_start_offset;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTEXT_MENU_PARAMS_H_
