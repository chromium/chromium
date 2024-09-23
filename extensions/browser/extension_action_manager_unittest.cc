// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_action_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// TODO(devlin): This really seems like more of an ExtensionAction test than
// an ExtensionActionManager test.
class ExtensionActionManagerTest
    : public ExtensionsTest,
      public testing::WithParamInterface<ActionInfo::Type> {
 public:
  ExtensionActionManagerTest();

  ExtensionActionManagerTest(const ExtensionActionManagerTest&) = delete;
  ExtensionActionManagerTest& operator=(const ExtensionActionManagerTest&) =
      delete;

 protected:
  // ExtensionsTest:
  void SetUp() override;

  ExtensionActionManager* manager() { return manager_; }
  ExtensionRegistry* registry() { return registry_; }

 private:
  raw_ptr<ExtensionRegistry, DanglingUntriaged> registry_;
  raw_ptr<ExtensionActionManager, DanglingUntriaged> manager_;
};

ExtensionActionManagerTest::ExtensionActionManagerTest() = default;

void ExtensionActionManagerTest::SetUp() {
  ExtensionsTest::SetUp();
  registry_ = ExtensionRegistry::Get(browser_context());
  manager_ = ExtensionActionManager::Get(browser_context());
}

// Tests that if no icons are specified in the extension's action, values from
// the "icons" key are used instead.
TEST_P(ExtensionActionManagerTest, TestPopulateMissingValues_Icons) {
  // Test that the largest icon from the extension's "icons" key is chosen as a
  // replacement for missing action default_icons keys. "48" should not be
  // replaced because "128" can always be used in its place.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestKey("icons", base::Value::Dict()
                                       .Set("48", "icon48.png")
                                       .Set("128", "icon128.png"))
          .SetManifestVersion(GetManifestVersionForActionType(GetParam()))
          .SetManifestKey(ActionInfo::GetManifestKeyForActionType(GetParam()),
                          base::Value::Dict())
          .Build();

  ASSERT_TRUE(extension);
  registry()->AddEnabled(extension);
  const ExtensionAction* action = manager()->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_EQ(GetParam(), action->action_type());

  ASSERT_TRUE(action->default_icon());
  // Since no icons are specified in the extension action, the product icons
  // (from the "icons" key) are used instead.
  EXPECT_EQ(action->default_icon()->map(),
            IconsInfo::GetIcons(extension.get()).map());
}

TEST_P(ExtensionActionManagerTest, TestPopulateMissingValues_Title) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(GetManifestVersionForActionType(GetParam()))
          .SetManifestKey(ActionInfo::GetManifestKeyForActionType(GetParam()),
                          base::Value::Dict())
          .Build();

  ASSERT_TRUE(extension);
  registry()->AddEnabled(extension);
  const ExtensionAction* action = manager()->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_EQ(GetParam(), action->action_type());

  EXPECT_EQ(extension->name(),
            action->GetTitle(ExtensionAction::kDefaultTabId));
}

// Tests that if defaults are provided in the extension action specification,
// those should be used.
TEST_P(ExtensionActionManagerTest, TestDontOverrideIfDefaultsProvided) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(GetManifestVersionForActionType(GetParam()))
          .SetManifestKey("icons", base::Value::Dict().Set("24", "icon24.png"))
          .SetManifestKey(ActionInfo::GetManifestKeyForActionType(GetParam()),
                          base::Value::Dict()
                              .Set("default_icon",
                                   base::Value::Dict().Set("19", "icon19.png"))
                              .Set("default_title", "Action!"))
          .Build();

  ASSERT_TRUE(extension);
  registry()->AddEnabled(extension);
  const ExtensionAction* action = manager()->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_EQ(GetParam(), action->action_type());

  ASSERT_TRUE(action->default_icon());
  // Since there was at least one icon specified in the extension action, the
  // action icon should use that.
  EXPECT_THAT(action->default_icon()->map(),
              testing::UnorderedElementsAre(std::make_pair(19, "icon19.png")));

  // Since the default_title was specified, it should be used.
  EXPECT_EQ("Action!", action->GetTitle(ExtensionAction::kDefaultTabId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionManagerTest,
                         testing::Values(ActionInfo::Type::kAction,
                                         ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage));

}  // namespace extensions
