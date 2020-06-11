// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/web_app_linked_shortcut_items.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

namespace errors = manifest_errors;

using WebAppLinkedShortcutItemsHandlerTest = ManifestTest;

TEST_F(WebAppLinkedShortcutItemsHandlerTest, Valid) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_linked_shortcut_items_valid.json",
                           extensions::Manifest::Location::INTERNAL,
                           extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidNotFromBookmarkApp) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_linked_shortcut_items_valid.json");
  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->from_bookmark());

  std::vector<InstallWarning> expected_install_warnings;
  expected_install_warnings.emplace_back(
      InstallWarning(errors::kInvalidWebAppLinkedShortcutItemsNotBookmarkApp));
  EXPECT_EQ(expected_install_warnings, extension->install_warnings());

  const WebAppLinkedShortcutItems& web_app_linked_shortcut_items =
      WebAppLinkedShortcutItems::GetWebAppLinkedShortcutItems(extension.get());
  EXPECT_TRUE(web_app_linked_shortcut_items.shortcut_item_infos.empty());
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItems) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid1.json",
                     errors::kInvalidWebAppLinkedShortcutItems,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItem) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid2.json",
                     errors::kInvalidWebAppLinkedShortcutItem,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemName) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid3.json",
                     errors::kInvalidWebAppLinkedShortcutItemName,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemUrl) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid4.json",
                     errors::kInvalidWebAppLinkedShortcutItemUrl,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemUrl2) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid10.json",
                     errors::kInvalidWebAppLinkedShortcutItemUrl,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemIcons) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid5.json",
                     errors::kInvalidWebAppLinkedShortcutItemIcons,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemIcon) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid6.json",
                     errors::kInvalidWebAppLinkedShortcutItemIcon,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, InvalidLinkedShortcutItemIconUrl) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid7.json",
                     errors::kInvalidWebAppLinkedShortcutItemIconUrl,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest,
       InvalidLinkedShortcutItemIconUrl2) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid8.json",
                     errors::kInvalidWebAppLinkedShortcutItemIconUrl,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest,
       InvalidLinkedShortcutItemIconSize) {
  LoadAndExpectError("web_app_linked_shortcut_items_invalid9.json",
                     errors::kInvalidWebAppLinkedShortcutItemIconSize,
                     extensions::Manifest::Location::INTERNAL,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest, ValidLinkedShortcutItemNoIcons) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_linked_shortcut_items_valid_no_icons.json",
                           extensions::Manifest::Location::INTERNAL,
                           extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());
}

TEST_F(WebAppLinkedShortcutItemsHandlerTest,
       ValidLinkedShortcutItemMultipleIcons) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "web_app_linked_shortcut_items_valid_multiple_icons.json",
      extensions::Manifest::Location::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());
}

}  // namespace extensions
