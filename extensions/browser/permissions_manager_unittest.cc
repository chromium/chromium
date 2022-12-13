// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  scoped_refptr<const Extension> AddExtensionWithHostPermission(
      const std::string& name,
      const std::string& host_permission);

  // Returns the restricted sites stored in `manager_`.
  std::set<url::Origin> GetRestrictedSitesFromManager();
  // Returns the permittes sites stored in `manager_`.
  std::set<url::Origin> GetPermittedSitesFromManager();

  // Returns the restricted sites stored in `extension_prefs_`.
  const base::Value* GetRestrictedSitesFromPrefs();
  // Returns the permitted sites stored in `extension_prefs_`.
  const base::Value* GetPermittedSitesFromPrefs();

  // Returns the restricted sites stored in `PermissionsData`.
  std::set<std::string> GetRestrictedSitesFromPermissionsData();
  // Returns the permitted sites stored in `PermissionsData`.
  std::set<std::string> GetPermittedSitesFromPermissionsData();

 protected:
  // ExtensionsTest:
  void SetUp() override;

  // PermissionsManager being tested.
  raw_ptr<PermissionsManager> manager_;

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

scoped_refptr<const Extension>
PermissionsManagerUnittest::AddExtensionWithHostPermission(
    const std::string& name,
    const std::string& host_permission) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .SetManifestKey(
              "host_permissions",
              extensions::ListBuilder().Append(host_permission).Build())
          .Build();

  ExtensionRegistryFactory::GetForBrowserContext(browser_context())
      ->AddEnabled(extension);

  return extension;
}

const base::Value* PermissionsManagerUnittest::GetRestrictedSitesFromPrefs() {
  const base::Value::Dict& permissions =
      extension_prefs_->GetPrefAsDictionary(kUserPermissions);
  return permissions.Find("restricted_sites");
}

const base::Value* PermissionsManagerUnittest::GetPermittedSitesFromPrefs() {
  const base::Value::Dict& permissions =
      extension_prefs_->GetPrefAsDictionary(kUserPermissions);
  return permissions.Find("permitted_sites");
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

std::set<std::string>
PermissionsManagerUnittest::GetRestrictedSitesFromPermissionsData() {
  std::set<std::string> string_patterns;
  URLPatternSet patterns = PermissionsData::GetUserBlockedHosts(
      util::GetBrowserContextId(browser_context()));
  for (const auto& pattern : patterns)
    string_patterns.insert(pattern.GetAsString());
  return string_patterns;
}

std::set<std::string>
PermissionsManagerUnittest::GetPermittedSitesFromPermissionsData() {
  std::set<std::string> string_patterns;
  URLPatternSet patterns = PermissionsData::GetUserAllowedHosts(
      util::GetBrowserContextId(browser_context()));
  for (const auto& pattern : patterns)
    string_patterns.insert(pattern.GetAsString());
  return string_patterns;
}

TEST_F(PermissionsManagerUnittest, AddAndRemoveRestrictedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  const std::string expected_url_pattern = "http://a.example.com/*";
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  base::Value value_with_url(base::Value::Type::LIST);
  value_with_url.Append(url.Serialize());

  // Verify the restricted sites list is empty.
  EXPECT_EQ(GetRestrictedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetRestrictedSitesFromPrefs(), nullptr);
  EXPECT_THAT(GetRestrictedSitesFromPermissionsData(), testing::IsEmpty());
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);

  // Add `url` to restricted sites. Verify the site is stored both in manager
  // and prefs restricted sites.
  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(), value_with_url);
  EXPECT_THAT(GetRestrictedSitesFromPermissionsData(),
              testing::UnorderedElementsAre(expected_url_pattern));
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kBlockAllExtensions);

  // Adding an existent restricted site. Verify the entry is not duplicated.
  manager_->AddUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(), value_with_url);
  EXPECT_THAT(GetRestrictedSitesFromPermissionsData(),
              testing::UnorderedElementsAre(expected_url_pattern));

  // Remove `url` from restricted sites. Verify the site is removed from both
  // manager and prefs restricted sites.
  manager_->RemoveUserRestrictedSite(url);
  EXPECT_EQ(GetRestrictedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(*GetRestrictedSitesFromPrefs(),
            base::Value(base::Value::Type::LIST));
  EXPECT_THAT(GetRestrictedSitesFromPermissionsData(), testing::IsEmpty());
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            UserSiteSetting::kCustomizeByExtension);
}

TEST_F(PermissionsManagerUnittest, AddAndRemovePermittedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  const std::string expected_url_pattern = "http://a.example.com/*";
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  base::Value value_with_url(base::Value::Type::LIST);
  value_with_url.Append(url.Serialize());

  // Verify the permitted sites list is empty.
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetPermittedSitesFromPrefs(), nullptr);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(), testing::IsEmpty());
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Add `url` to permitted sites. Verify the site is stored both in manager
  // and prefs permitted sites.
  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetPermittedSitesFromPrefs(), value_with_url);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(),
              testing::UnorderedElementsAre(expected_url_pattern));
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kGrantAllExtensions);

  // Adding an existent permitted site. Verify the entry is not duplicated.
  manager_->AddUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetPermittedSitesFromPrefs(), value_with_url);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(),
              testing::UnorderedElementsAre(expected_url_pattern));

  // Remove `url` from permitted sites. Verify the site is removed from both
  // manager and prefs permitted sites.
  manager_->RemoveUserPermittedSite(url);
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(*GetPermittedSitesFromPrefs(),
            base::Value(base::Value::Type::LIST));
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(), testing::IsEmpty());
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

TEST_F(PermissionsManagerUnittest, UpdateUserSiteSetting) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> empty_set;
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);

  {
    manager_->UpdateUserSiteSetting(
        url, PermissionsManager::UserSiteSetting::kGrantAllExtensions);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, empty_set);
    EXPECT_EQ(actual_permissions.permitted_sites, set_with_url);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kGrantAllExtensions);
  }

  {
    manager_->UpdateUserSiteSetting(
        url, PermissionsManager::UserSiteSetting::kBlockAllExtensions);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, set_with_url);
    EXPECT_EQ(actual_permissions.permitted_sites, empty_set);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  }

  {
    manager_->UpdateUserSiteSetting(
        url, PermissionsManager::UserSiteSetting::kCustomizeByExtension);
    const PermissionsManager::UserPermissionsSettings& actual_permissions =
        manager_->GetUserPermissionsSettings();
    EXPECT_EQ(actual_permissions.restricted_sites, empty_set);
    EXPECT_EQ(actual_permissions.permitted_sites, empty_set);
    EXPECT_EQ(manager_->GetUserSiteSetting(url),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  }
}

TEST_F(PermissionsManagerUnittest, GetSiteAccess_AllUrls) {
  auto extension =
      AddExtensionWithHostPermission("AllUrls Extension", "<all_urls>");

  const GURL non_restricted_url("https://www.non-restricted.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, non_restricted_url);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  // Chrome pages should be restricted, and the extension shouldn't have grant
  // or withheld site access.
  const GURL restricted_url("chrome://extensions");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, restricted_url);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

TEST_F(PermissionsManagerUnittest, GetSiteAccess_RequestedUrl) {
  auto extension = AddExtensionWithHostPermission("RequestedUrl Extension",
                                                  "*://*.requested.com/*");

  const GURL requested_url("https://www.requested.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, requested_url);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  const GURL non_requested_url("https://non-requested.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, non_requested_url);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// Tests that for the purposes of displaying an extension's site access to the
// user (or granting/revoking permissions), we ignore paths in the URL. We
// always strip the path from host permissions directly, but we don't strip the
// path from content scripts.
TEST_F(PermissionsManagerUnittest,
       GetSiteAccess_ContentScript_RequestedUrlWithPath) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddContentScript("foo.js", {"https://www.example.com/foo"})
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  ExtensionRegistryFactory::GetForBrowserContext(browser_context())
      ->AddEnabled(extension);

  const GURL other_path_url("https://www.example.com/bar");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, other_path_url);
    // Even though the path doesn't match the one requested, the domain does
    // match and thus we treat it as if the site was requested.
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

TEST_F(PermissionsManagerUnittest, GetSiteAccess_ActiveTab) {
  auto extension =
      AddExtensionWithHostPermission("ActiveTab Extension", "activeTab");

  const GURL url("https://example.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, url);
    // The site access computation does not take into account active tab, and
    // therefore it does not have or withheld any access.
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

TEST_F(PermissionsManagerUnittest, GetSiteAccess_NoHostPermissions) {
  auto extension = AddExtensionWithHostPermission("Test", "Extension");

  const GURL url("https://example.com");
  {
    const PermissionsManager::ExtensionSiteAccess site_access =
        manager_->GetSiteAccess(*extension, url);
    // The site access computation does not take into account active tab, and
    // therefore it does not have or withheld any access.
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

TEST_F(PermissionsManagerUnittest, CanAffectExtension_ByLocation) {
  struct {
    mojom::ManifestLocation location;
    bool can_be_affected;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, true},
      {mojom::ManifestLocation::kExternalPref, true},
      {mojom::ManifestLocation::kUnpacked, true},
      {mojom::ManifestLocation::kExternalPolicyDownload, false},
      {mojom::ManifestLocation::kComponent, false},
  };

  for (const auto& test_case : test_cases) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(test_case.location)
            .AddPermission("<all_urls>")
            .Build();
    EXPECT_EQ(manager_->CanAffectExtension(*extension),
              test_case.can_be_affected)
        << test_case.location;
  }
}

}  // namespace extensions
