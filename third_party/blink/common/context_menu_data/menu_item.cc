// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/menu_item.h"

namespace blink {

MenuItem::MenuItem()
    : type(OPTION),
      action(0),
      rtl(false),
      has_directional_override(false),
      enabled(false),
      checked(false) {}

MenuItem::MenuItem(const MenuItem& item)
    : label(item.label),
      icon(item.icon),
      tool_tip(item.tool_tip),
      type(item.type),
      action(item.action),
      rtl(item.rtl),
      has_directional_override(item.has_directional_override),
      enabled(item.enabled),
      checked(item.checked),
      submenu(item.submenu) {}

MenuItem::~MenuItem() {}

MenuItem MenuItemBuilder::Build(const blink::MenuItemInfo& item) {
  blink::MenuItem result;

  result.label = item.label;
  result.tool_tip = item.tool_tip;
  result.type = static_cast<MenuItem::Type>(item.type);
  result.action = item.action;
  result.rtl = (item.text_direction == base::i18n::RIGHT_TO_LEFT);
  result.has_directional_override = item.has_text_direction_override;
  result.enabled = item.enabled;
  result.checked = item.checked;
  for (size_t i = 0; i < item.sub_menu_items.size(); ++i)
    result.submenu.push_back(MenuItemBuilder::Build(item.sub_menu_items[i]));

  return result;
}

}  // namespace blink
