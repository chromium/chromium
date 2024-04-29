// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class PageActionManifestTest : public ManifestTest {
 protected:
  std::unique_ptr<ActionInfo> LoadAction(const std::string& manifest_filename);
};

std::unique_ptr<ActionInfo> PageActionManifestTest::LoadAction(
    const std::string& manifest_filename) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      manifest_filename.c_str());
  const ActionInfo* page_action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kPage);
  EXPECT_TRUE(page_action_info);
  if (page_action_info)
    return std::make_unique<ActionInfo>(*page_action_info);
  ADD_FAILURE() << "Expected manifest in " << manifest_filename
                << " to include a page_action section.";
  return nullptr;
}

TEST_F(PageActionManifestTest, ManifestVersion2DoesntAllowLegacyKeys) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("page_action_manifest_version_2.json"));
  ASSERT_TRUE(extension.get());
  const ActionInfo* page_action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kPage);
  ASSERT_TRUE(page_action_info);

  EXPECT_TRUE(page_action_info->default_icon.empty());
  EXPECT_EQ("", page_action_info->default_title);
  EXPECT_TRUE(page_action_info->default_popup_url.is_empty());

  LoadAndExpectError("page_action_manifest_version_2b.json",
                     errors::kInvalidActionDefaultPopup);
}

TEST_F(PageActionManifestTest, LoadPageActionHelper) {
  std::unique_ptr<ActionInfo> action;

  // First try with an empty dictionary.
  action = LoadAction("page_action_empty.json");
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->default_icon.empty());
  EXPECT_EQ("", action->default_title);

  // Now setup some values to use in the action.
  const std::string kTitle("MyExtensionActionTitle");
  const std::string kIcon("image1.png");

  action = LoadAction("page_action.json");
  ASSERT_TRUE(action);

  EXPECT_EQ(kTitle, action->default_title);
  EXPECT_EQ(kIcon,
            action->default_icon.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                                     ExtensionIconSet::Match::kSmaller));

  // Now test that we can parse the new format for page actions.
  const std::string kPopupHtmlFile("a_popup.html");

  // Invalid title should give an error.
  LoadAndExpectError("page_action_invalid_title.json",
                     errors::kInvalidActionDefaultTitle);

  // Test the "default_popup" key.
  // These tests require an extension_url, so we also load the extension.

  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("page_action_default_popup.json");
  const ActionInfo* extension_action =
      GetActionInfoOfType(*extension, ActionInfo::Type::kPage);
  ASSERT_TRUE(extension_action);
  EXPECT_EQ(extension->url().Resolve(kPopupHtmlFile),
            extension_action->default_popup_url);

  // Setting default_popup to "" is the same as having no popup.
  action = LoadAction("page_action_empty_default_popup.json");
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->default_popup_url.is_empty());
}

}  // namespace extensions
