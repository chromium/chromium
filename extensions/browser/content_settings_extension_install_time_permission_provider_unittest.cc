// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_settings_extension_install_time_permission_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

void AddPermission(const Extension* extension, mojom::APIPermissionID id) {
  APIPermissionSet apis;
  apis.insert(id);
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(std::move(apis), ManifestPermissionSet(),
                                      URLPatternSet(), URLPatternSet()),
      std::make_unique<PermissionSet>());
}

class ExtensionInstallTimePermissionProviderTest : public ExtensionsTest {
 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();
    registry_ = ExtensionRegistry::Get(browser_context());
    provider_ = std::make_unique<ExtensionInstallTimePermissionProvider>(
        browser_context(), registry_);

    // Ensure geolocation and notifications are registered.
    PermissionsInfo* info = PermissionsInfo::GetInstance();
    if (!info->GetByID(mojom::APIPermissionID::kGeolocation)) {
      info->RegisterPermissions(
          {{mojom::APIPermissionID::kGeolocation, "geolocation"}}, {});
    }
    if (!info->GetByID(mojom::APIPermissionID::kNotifications)) {
      info->RegisterPermissions(
          {{mojom::APIPermissionID::kNotifications, "notifications"}}, {});
    }
  }

  void TearDown() override {
    provider_->ShutdownOnUIThread();
    provider_.reset();
    registry_ = nullptr;
    ExtensionsTest::TearDown();
  }

  ExtensionRegistry* registry() { return registry_; }
  ExtensionInstallTimePermissionProvider* provider() { return provider_.get(); }

 private:
  raw_ptr<ExtensionRegistry> registry_;
  std::unique_ptr<ExtensionInstallTimePermissionProvider> provider_;
};

TEST_F(ExtensionInstallTimePermissionProviderTest, GetRule) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension").Build();
  AddPermission(extension.get(), mojom::APIPermissionID::kGeolocation);
  ASSERT_TRUE(extension);
  registry()->AddEnabled(extension);
  registry()->TriggerOnLoaded(extension.get());

  // The primary URL must exactly match the extension's URL.
  GURL extension_url = extension->url();

  std::unique_ptr<content_settings::Rule> rule = provider()->GetRule(
      extension_url, extension_url, ContentSettingsType::GEOLOCATION,
      /*off_the_record=*/false);

  ASSERT_TRUE(rule);
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(extension_url),
            rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), rule->value);

  // Different ContentSettingsType that the extension doesn't have the API
  // permission for.
  std::unique_ptr<content_settings::Rule> no_rule = provider()->GetRule(
      extension_url, extension_url, ContentSettingsType::NOTIFICATIONS,
      /*off_the_record=*/false);
  EXPECT_FALSE(no_rule);

  // Unrelated ContentSettingsType.
  std::unique_ptr<content_settings::Rule> unrelated_rule = provider()->GetRule(
      extension_url, extension_url, ContentSettingsType::COOKIES,
      /*off_the_record=*/false);
  EXPECT_FALSE(unrelated_rule);

  // URL not an extension.
  std::unique_ptr<content_settings::Rule> url_rule = provider()->GetRule(
      GURL("https://example.com"), GURL("https://example.com"),
      ContentSettingsType::GEOLOCATION,
      /*off_the_record=*/false);
  EXPECT_FALSE(url_rule);
}

TEST_F(ExtensionInstallTimePermissionProviderTest, GetRuleIterator) {
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder("Extension 1").Build();
  AddPermission(extension1.get(), mojom::APIPermissionID::kGeolocation);
  registry()->AddEnabled(extension1);
  registry()->TriggerOnLoaded(extension1.get());

  scoped_refptr<const Extension> extension2 =
      ExtensionBuilder("Extension 2").Build();
  registry()->AddEnabled(extension2);
  registry()->TriggerOnLoaded(extension2.get());

  scoped_refptr<const Extension> extension3 =
      ExtensionBuilder("Extension 3").Build();
  AddPermission(extension3.get(), mojom::APIPermissionID::kGeolocation);
  registry()->AddEnabled(extension3);
  registry()->TriggerOnLoaded(extension3.get());

  // Expect exactly two extensions (extension1, extension3) have GEOLOCATION.
  std::unique_ptr<content_settings::RuleIterator> iterator =
      provider()->GetRuleIterator(ContentSettingsType::GEOLOCATION,
                                  /*off_the_record=*/false);

  ASSERT_TRUE(iterator);
  int count = 0;
  while (iterator->HasNext()) {
    std::unique_ptr<content_settings::Rule> rule = iterator->Next();
    ASSERT_TRUE(rule);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
    EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), rule->value);
    count++;
  }
  EXPECT_EQ(2, count);
  // reset iterator to release lock.
  iterator.reset();

  // Expect zero extensions have NOTIFICATIONS (null iterator).
  iterator = provider()->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                         /*off_the_record=*/false);
  ASSERT_FALSE(iterator);

  // Expect null iterator for unrelated ContentSettingsType.
  iterator = provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                         /*off_the_record=*/false);
  EXPECT_FALSE(iterator);
}

TEST_F(ExtensionInstallTimePermissionProviderTest, NoopMethods) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension").Build();
  AddPermission(extension.get(), mojom::APIPermissionID::kGeolocation);
  registry()->AddEnabled(extension);
  registry()->TriggerOnLoaded(extension.get());

  // SetWebsiteSetting should return false (not supported).
  EXPECT_FALSE(provider()->SetWebsiteSetting(
      ContentSettingsPattern::FromURLNoWildcard(extension->url()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::GEOLOCATION,
      base::Value(CONTENT_SETTING_ALLOW), {}));

  // ClearAllContentSettingsRules should not crash.
  provider()->ClearAllContentSettingsRules(ContentSettingsType::GEOLOCATION);
}

}  // namespace extensions
