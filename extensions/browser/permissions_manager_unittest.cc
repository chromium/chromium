// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/pref_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

std::unique_ptr<KeyedService> SetTestingPermissionsManager(
    content::BrowserContext* browser_context) {
  return std::make_unique<extensions::PermissionsManager>(browser_context);
}

}  // namespace

namespace extensions {

using UserSiteSetting = PermissionsManager::UserSiteSetting;

class PermissionsManagerUnittest : public ExtensionsTest {
 public:
  PermissionsManagerUnittest() = default;
  ~PermissionsManagerUnittest() override = default;
  PermissionsManagerUnittest(const PermissionsManagerUnittest&) = delete;
  PermissionsManagerUnittest& operator=(const PermissionsManagerUnittest&) =
      delete;

  // Returns the restricted sites stored in `manager_`.
  std::set<url::Origin> GetRestrictedSitesFromManager();
  // Returns the permittes sites stored in `manager_`.
  std::set<url::Origin> GetPermittedSitesFromManager();

  // Returns the restricted sites stored in `extension_prefs_`.
  const base::Value* GetRestrictedSitesFromPrefs();
  // Returns the permitted sites stored in `extension_prefs_`.
  const base::Value* GetPermittedSitesFromPrefs();

 protected:
  // ExtensionsTest:
  void SetUp() override;

  // PermissionsManager being tested.
  PermissionsManager* manager_;

  raw_ptr<ExtensionPrefs> extension_prefs_;
};

void PermissionsManagerUnittest::SetUp() {
  ExtensionsTest::SetUp();
  manager_ = static_cast<PermissionsManager*>(
      PermissionsManager::GetFactory()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&SetTestingPermissionsManager)));

  extension_prefs_ = ExtensionPrefs::Get(browser_context());
}

const base::Value* PermissionsManagerUnittest::GetRestrictedSitesFromPrefs() {
  const base::DictionaryValue* permissions =
      extension_prefs_->GetPrefAsDictionary(kUserPermissions);
  return permissions->FindKey("restricted_sites");
}

const base::Value* PermissionsManagerUnittest::GetPermittedSitesFromPrefs() {
  const base::DictionaryValue* permissions =
      extension_prefs_->GetPrefAsDictionary(kUserPermissions);
  return permissions->FindKey("permitted_sites");
}

std::set<url::Origin>
PermissionsManagerUnittest::GetRestrictedSitesFromManager() {
  const PermissionsManager::UserPermissionsSettings& permissions =
      manager_->GetUserPermissionsSettings();
  return permissions.restricted_sites;
}

std::set<url::Origin>
PermissionsManagerUnittest::GetPermittedSitesFromManager() {
  const PermissionsManager::UserPermissionsSettings& permissions =
      manager_->GetUserPermissionsSettings();
  return permissions.permitted_sites;
}

TEST_F(PermissionsManagerUnittest, AddAndRemoveRestrictedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  base::Value value_with_url(base::Value::Type::LIST);
  value_with_url.Append(url.Serialize());

  // Verify the restricted sites list is empty.
  EXPECT_EQ(GetRestrictedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetRestrictedSitesFromPrefs(), nullptr);
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);

  // Add `url` to restricted sites. Verify the site is stored both in manager
  // and prefs restricted sites.
  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(), value_with_url);
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kBlockAllExtensions);

  // Adding an existent restricted site. Verify the entry is not duplicated.
  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(), value_with_url);

  // Remove `url` from restricted sites. Verify the site is removed from both
  // manager and prefs restricted sites.
  manager_->RemoveUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(),
            base::Value(base::Value::Type::LIST));
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);
}

TEST_F(PermissionsManagerUnittest, AddAndRemovePermittedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  base::Value value_with_url(base::Value::Type::LIST);
  value_with_url.Append(url.Serialize());

  // Verify the permitted sites list is empty.
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetPermittedSitesFromPrefs(), nullptr);
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Add `url` to permitted sites. Verify the site is stored both in manager
  // and prefs permitted sites.
  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetPermittedSitesFromPrefs(), value_with_url);
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kGrantAllExtensions);

  // Adding an existent permitted site. Verify the entry is not duplicated.
  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetPermittedSitesFromPrefs(), value_with_url);

  // Remove `url` from permitted sites. Verify the site is removed from both
  // manager and prefs permitted sites.
  manager_->RemoveUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(*GetPermittedSitesFromPrefs(),
            base::Value(base::Value::Type::LIST));
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
}

TEST_F(PermissionsManagerUnittest,
       RestrictedAndPermittedSitesAreMutuallyExclusive) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> empty_set;
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);

  {
    manager_->AddUserRestrictedSite(url);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, set_with_url);
    EXPECT_EQ(actual_permissions.permitted_sites, empty_set);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  }

  {
    // Adding an url to the permitted sites that is already in the restricted
    // sites should remove it from restricted sites and add it to permitted
    // sites.
    manager_->AddUserPermittedSite(url);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, empty_set);
    EXPECT_EQ(actual_permissions.permitted_sites, set_with_url);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kGrantAllExtensions);
  }

  {
    // Adding an url to the restricted sites that is already in the permitted
    // sites should remove it from permitted sites and add it to restricted
    // sites.
    manager_->AddUserRestrictedSite(url);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, set_with_url);
    EXPECT_EQ(actual_permissions.permitted_sites, empty_set);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  }
}

}  // namespace extensions
