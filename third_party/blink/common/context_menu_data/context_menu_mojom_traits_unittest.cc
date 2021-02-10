// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/context_menu_data/context_menu_mojom_traits.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/menu_item.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

namespace blink {

TEST(MenuItemStructTraitsTest, MenuItemRoundtrip) {
  MenuItem menu_item;
  menu_item.label = base::ASCIIToUTF16("Label of menu item");
  menu_item.icon = base::ASCIIToUTF16("Icon of menu item");
  menu_item.tool_tip = base::ASCIIToUTF16("Tooltip of menu item");
  menu_item.type = MenuItem::Type::GROUP;
  menu_item.action = 1;
  menu_item.rtl = false;
  menu_item.has_directional_override = true;
  menu_item.enabled = true;
  menu_item.checked = true;

  menu_item.submenu.resize(2);
  menu_item.submenu[0].label = base::ASCIIToUTF16("Label of submenu item 1");
  menu_item.submenu[0].icon = base::ASCIIToUTF16("Icon of submenu item 1");
  menu_item.submenu[0].tool_tip =
      base::ASCIIToUTF16("Tooltip of submenu item 1");
  menu_item.submenu[0].type = MenuItem::Type::SUBMENU;
  menu_item.submenu[0].action = 2;
  menu_item.submenu[1].label = base::ASCIIToUTF16("Label of submenu item 2");
  menu_item.submenu[1].icon = base::ASCIIToUTF16("Icon of submenu item 2");
  menu_item.submenu[0].tool_tip =
      base::ASCIIToUTF16("Tooltip of submenu item 2");
  menu_item.submenu[1].type = MenuItem::Type::SUBMENU;
  menu_item.submenu[1].action = 2;

  MenuItem roundtrip_menu_item;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::CustomContextMenuItem>(
          menu_item, roundtrip_menu_item));

  EXPECT_EQ(roundtrip_menu_item.label, menu_item.label);
  EXPECT_EQ(roundtrip_menu_item.icon, menu_item.icon);
  EXPECT_EQ(roundtrip_menu_item.tool_tip, menu_item.tool_tip);
  EXPECT_EQ(roundtrip_menu_item.type, menu_item.type);
  EXPECT_EQ(roundtrip_menu_item.action, menu_item.action);
  EXPECT_EQ(roundtrip_menu_item.rtl, menu_item.rtl);
  EXPECT_EQ(roundtrip_menu_item.has_directional_override,
            menu_item.has_directional_override);
  EXPECT_EQ(roundtrip_menu_item.enabled, menu_item.enabled);
  EXPECT_EQ(roundtrip_menu_item.checked, menu_item.checked);

  for (size_t i = 0; i < menu_item.submenu.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Submenu index: %zd", i));
    EXPECT_EQ(menu_item.submenu[i].label, roundtrip_menu_item.submenu[i].label);
    EXPECT_EQ(menu_item.submenu[i].icon, roundtrip_menu_item.submenu[i].icon);
    EXPECT_EQ(menu_item.submenu[i].tool_tip,
              roundtrip_menu_item.submenu[i].tool_tip);
    EXPECT_EQ(menu_item.submenu[i].type, roundtrip_menu_item.submenu[i].type);
    EXPECT_EQ(menu_item.submenu[i].action,
              roundtrip_menu_item.submenu[i].action);
  }
}

}  // namespace blink
