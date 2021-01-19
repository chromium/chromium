// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_H_

#include <vector>

#include "base/strings/string16.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"

namespace blink {

// Container for information about entries in an HTML select popup menu and
// custom entries of the context menu.
struct BLINK_COMMON_EXPORT MenuItem {
  enum Type {
    OPTION = blink::MenuItemInfo::kOption,
    CHECKABLE_OPTION = blink::MenuItemInfo::kCheckableOption,
    GROUP = blink::MenuItemInfo::kGroup,
    SEPARATOR = blink::MenuItemInfo::kSeparator,
    SUBMENU,  // This is currently only used by Pepper, not by Blink.
    TYPE_LAST = SUBMENU
  };

  MenuItem();
  MenuItem(const MenuItem& item);
  ~MenuItem();

  base::string16 label;
  base::string16 icon;
  base::string16 tool_tip;
  Type type;
  unsigned action;
  bool rtl;
  bool has_directional_override;
  bool enabled;
  bool checked;
  std::vector<MenuItem> submenu;
};

class BLINK_COMMON_EXPORT MenuItemBuilder {
 public:
  static blink::MenuItem Build(const blink::MenuItemInfo& item);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_H_
