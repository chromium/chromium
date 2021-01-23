// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"

#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"

namespace blink {

const int32_t CustomContextMenuContext::kCurrentRenderWidget = INT32_MAX;

CustomContextMenuContext::CustomContextMenuContext()
    : is_pepper_menu(false),
      request_id(0),
      render_widget_id(kCurrentRenderWidget) {}

UntrustworthyContextMenuParams::UntrustworthyContextMenuParams()
    : media_type(blink::mojom::ContextMenuDataMediaType::kNone),
      x(0),
      y(0),
      has_image_contents(false),
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
      input_field_type(blink::mojom::ContextMenuDataInputFieldType::kNone),
      selection_start_offset(0) {}

UntrustworthyContextMenuParams::UntrustworthyContextMenuParams(
    const UntrustworthyContextMenuParams& other) = default;

UntrustworthyContextMenuParams::~UntrustworthyContextMenuParams() = default;

}  // namespace blink
