// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/context_menu_params.h"

namespace content {

const int32_t CustomContextMenuContext::kCurrentRenderWidget = INT32_MAX;

CustomContextMenuContext::CustomContextMenuContext()
    : is_pepper_menu(false),
      request_id(0),
      render_widget_id(kCurrentRenderWidget) {
}

ContextMenuParams::ContextMenuParams()
    : media_type(blink::ContextMenuDataMediaType::kNone),
      x(0),
      y(0),
      has_image_contents(false),
      media_flags(0),
      spellcheck_enabled(false),
      is_editable(false),
      writing_direction_default(
          blink::WebContextMenuData::kCheckableMenuItemDisabled),
      writing_direction_left_to_right(
          blink::WebContextMenuData::kCheckableMenuItemEnabled),
      writing_direction_right_to_left(
          blink::WebContextMenuData::kCheckableMenuItemEnabled),
      edit_flags(0),
      referrer_policy(network::mojom::ReferrerPolicy::kDefault),
      source_type(ui::MENU_SOURCE_NONE),
      input_field_type(blink::ContextMenuDataInputFieldType::kNone),
      selection_start_offset(0) {}

ContextMenuParams::ContextMenuParams(const ContextMenuParams& other) = default;

ContextMenuParams::~ContextMenuParams() {
}

}  // namespace content
