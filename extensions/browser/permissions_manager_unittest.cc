// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/extensions_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

std::unique_ptr<KeyedService> SetTestingPermissionsManager(
    content::BrowserContext* browser_context) {
  return std::make_unique<extensions::PermissionsManager>();
}

}  // namespace

namespace extensions {

class PermissionsManagerUnittest : public ExtensionsTest {
 public:
  PermissionsManagerUnittest() = default;
  ~PermissionsManagerUnittest() override = default;
  PermissionsManagerUnittest(const PermissionsManagerUnittest&) = delete;
  PermissionsManagerUnittest& operator=(const PermissionsManagerUnittest&) =
      delete;

 protected:
  // ExtensionsTest:
  void SetUp() override;

  // PermissionsManager being tested.
  PermissionsManager* manager_;
};

void PermissionsManagerUnittest::SetUp() {
  ExtensionsTest::SetUp();
  manager_ = static_cast<PermissionsManager*>(
      PermissionsManager::GetFactory()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&SetTestingPermissionsManager)));
}

bool operator==(const PermissionsManager::UserPermissionsSettings& lhs,
                const PermissionsManager::UserPermissionsSettings& rhs) {
  return ((lhs.restricted_sites == rhs.restricted_sites) &&
          (lhs.permitted_sites == rhs.permitted_sites));
}

TEST_F(PermissionsManagerUnittest, AddAndRemoveRestrictedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  PermissionsManager::UserPermissionsSettings expected_permissions;
  expected_permissions.restricted_sites.insert(url);

  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(), expected_permissions);

  // Adding an existent restricted site doesn't duplicate the entry.
  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(), expected_permissions);

  manager_->RemoveUserRestrictedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(),
            PermissionsManager::UserPermissionsSettings());
}

TEST_F(PermissionsManagerUnittest, AddAndRemovePermittedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  PermissionsManager::UserPermissionsSettings expected_permissions;
  expected_permissions.permitted_sites.insert(url);

  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(), expected_permissions);

  // Adding an existent permitted site doesn't duplicate the entry.
  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(), expected_permissions);

  manager_->RemoveUserPermittedSite(url);
  EXPECT_EQ(manager_->GetUserPermissionsSettings(),
            PermissionsManager::UserPermissionsSettings());
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
  }
}

}  // namespace extensions
