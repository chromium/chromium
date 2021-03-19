// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/web_app_shortcut_icons_handler.h"
#include "extensions/common/manifest_test.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace errors = manifest_errors;

using WebAppShortcutIconsHandlerTest = ManifestTest;

TEST_F(WebAppShortcutIconsHandlerTest, Valid) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "web_app_shortcut_icons_valid.json", ManifestLocation::kInternal,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());
}

TEST_F(WebAppShortcutIconsHandlerTest, InvalidNotFromBookmarkApp) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("web_app_shortcut_icons_valid.json");

  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->from_bookmark());

  std::vector<InstallWarning> expected_install_warnings;
  expected_install_warnings.push_back(
      InstallWarning(errors::kInvalidWebAppShortcutIconsNotBookmarkApp));
  EXPECT_EQ(expected_install_warnings, extension->install_warnings());

  const std::map<int, ExtensionIconSet>& web_app_shortcut_icons =
      WebAppShortcutIconsInfo::GetShortcutIcons(extension.get());
  ASSERT_TRUE(web_app_shortcut_icons.empty());
}

TEST_F(WebAppShortcutIconsHandlerTest, InvalidShortcutIcons) {
  LoadAndExpectError("web_app_shortcut_icons_invalid1.json",
                     errors::kInvalidWebAppShortcutIcons,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppShortcutIconsHandlerTest, InvalidShortcutItemIcons) {
  LoadAndExpectError("web_app_shortcut_icons_invalid2.json",
                     errors::kInvalidWebAppShortcutItemIcons,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppShortcutIconsHandlerTest, InvalidShortcutItemIconKey) {
  LoadAndExpectError("web_app_shortcut_icons_invalid3.json",
                     errors::kInvalidIconKey, ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

TEST_F(WebAppShortcutIconsHandlerTest, InvalidShortcutItemIconPath) {
  LoadAndExpectError("web_app_shortcut_icons_invalid4.json",
                     errors::kInvalidIconPath, ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

}  // namespace extensions
