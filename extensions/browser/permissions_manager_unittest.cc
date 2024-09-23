// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/permissions_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
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
using UserSiteAccess = PermissionsManager::UserSiteAccess;

class PermissionsManagerUnittest : public ExtensionsTest {
 public:
  PermissionsManagerUnittest() = default;
  ~PermissionsManagerUnittest() override = default;
  PermissionsManagerUnittest(const PermissionsManagerUnittest&) = delete;
  PermissionsManagerUnittest& operator=(const PermissionsManagerUnittest&) =
      delete;

  scoped_refptr<const Extension> AddExtension(const std::string& name);
  scoped_refptr<const Extension> AddExtensionWithAPIPermission(
      const std::string& name,
      const std::string& permission,
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kUnpacked);
  scoped_refptr<const Extension> AddExtensionWithHostPermission(
      const std::string& name,
      const std::string& host_permission);
  scoped_refptr<const Extension> AddExtensionWithActiveTab(
      const std::string& name,
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kUnpacked);

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
  raw_ptr<PermissionsManager, DanglingUntriaged> manager_;

  raw_ptr<ExtensionPrefs, DanglingUntriaged> extension_prefs_;
};

void PermissionsManagerUnittest::SetUp() {
  ExtensionsTest::SetUp();
  manager_ = static_cast<PermissionsManager*>(
      PermissionsManager::GetFactory()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&SetTestingPermissionsManager)));

  extension_prefs_ = ExtensionPrefs::Get(browser_context());
}

scoped_refptr<const Extension> PermissionsManagerUnittest::AddExtension(
    const std::string& name) {
  return AddExtensionWithHostPermission(name, "");
}

scoped_refptr<const Extension>
PermissionsManagerUnittest::AddExtensionWithAPIPermission(
    const std::string& name,
    const std::string& permission,
    extensions::mojom::ManifestLocation location) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .AddAPIPermission(permission)
          .SetLocation(location)
          .Build();
  DCHECK(extension->permissions_data()->HasAPIPermission(permission));

  ExtensionRegistryFactory::GetForBrowserContext(browser_context())
      ->AddEnabled(extension);

  return extension;
}

scoped_refptr<const Extension>
PermissionsManagerUnittest::AddExtensionWithHostPermission(
    const std::string& name,
    const std::string& host_permission) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .AddHostPermission(host_permission)
          .Build();

  ExtensionRegistryFactory::GetForBrowserContext(browser_context())
      ->AddEnabled(extension);

  return extension;
}

scoped_refptr<const Extension>
PermissionsManagerUnittest::AddExtensionWithActiveTab(
    const std::string& name,
    extensions::mojom::ManifestLocation location) {
  return AddExtensionWithAPIPermission(name, "activeTab", location);
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
  base::Value::List value_with_url;
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
  // Verify the permitted sites list is empty.
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetPermittedSitesFromPrefs(), nullptr);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(), testing::IsEmpty());

  // Adding or removing a permitted site is only supported when
  // kExtensionsMenuAccessControlWithPermittedSites is enabled.
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  EXPECT_DCHECK_DEATH(manager_->AddUserPermittedSite(url));
  EXPECT_DCHECK_DEATH(manager_->RemoveUserPermittedSite(url));
}

TEST_F(PermissionsManagerUnittest, UpdateUserSiteSetting) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> empty_set;
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);

  {
    // Granting all extensions is only supported when
    // kExtensionsMenuAccessControlWithPermittedSites flag is enabled.
    EXPECT_DCHECK_DEATH(manager_->UpdateUserSiteSetting(
        url, PermissionsManager::UserSiteSetting::kGrantAllExtensions));
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
  auto extension = AddExtensionWithActiveTab("ActiveTab Extension");

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
  auto extension = AddExtension("Test");

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
            .AddHostPermission("<all_urls>")
            .Build();
    EXPECT_EQ(manager_->CanAffectExtension(*extension),
              test_case.can_be_affected)
        << test_case.location;
  }
}

TEST_F(PermissionsManagerUnittest, CanUserSelectSiteAccess_AllUrls) {
  auto extension =
      AddExtensionWithHostPermission("AllUrls Extension", "<all_urls>");

  // Verify "on click", "on site" and "on all sites" site access can be selected
  // for a non-restricted url.
  const GURL url("http://www.example.com");
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                UserSiteAccess::kOnClick));
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                UserSiteAccess::kOnSite));
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                UserSiteAccess::kOnAllSites));

  // Verify "on click", "on site" and "on all sites" cannot be selected for a
  // restricted url.
  const GURL chrome_url("chrome://settings");
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, chrome_url,
                                                 UserSiteAccess::kOnClick));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, chrome_url,
                                                 UserSiteAccess::kOnSite));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, chrome_url,
                                                 UserSiteAccess::kOnAllSites));
}

TEST_F(PermissionsManagerUnittest, CanUserSelectSiteAccess_SpecificUrl) {
  const GURL url_a("http://www.a.com");
  auto extension = AddExtensionWithHostPermission("A Extension", url_a.spec());

  // Verify "on click" and "on site" can be selected for the specific url, but
  // "on all sites" cannot be selected.
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url_a,
                                                UserSiteAccess::kOnClick));
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url_a,
                                                UserSiteAccess::kOnSite));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url_a,
                                                 UserSiteAccess::kOnAllSites));

  // Verify "on click", "on site" and "on all sites" cannot be selected for any
  // other url.
  const GURL url_b("http://www.b.com");
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url_b,
                                                 UserSiteAccess::kOnClick));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url_b,
                                                 UserSiteAccess::kOnSite));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url_b,
                                                 UserSiteAccess::kOnAllSites));
}

TEST_F(PermissionsManagerUnittest, CanUserSelectSiteAccess_NoHostPermissions) {
  auto extension = AddExtension("Extension");

  // Verify "on click", "on site" and "on all sites" cannot be selected for any
  // url.
  const GURL url("http://www.example.com");
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                 UserSiteAccess::kOnClick));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                 UserSiteAccess::kOnSite));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                 UserSiteAccess::kOnAllSites));
}

TEST_F(PermissionsManagerUnittest, CanUserSelectSiteAccess_ActiveTab) {
  auto extension = AddExtensionWithActiveTab("ActiveTab Extension");

  // Verify "on click" can be selected for the specific url, but "on site" and
  // "on all sites" cannot be selected.
  const GURL url("http://www.example.com");
  EXPECT_TRUE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                UserSiteAccess::kOnClick));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                 UserSiteAccess::kOnSite));
  EXPECT_FALSE(manager_->CanUserSelectSiteAccess(*extension, url,
                                                 UserSiteAccess::kOnAllSites));
}

TEST_F(PermissionsManagerUnittest, HasActiveTabAndCanAccess_PolicyUrl) {
  auto extension = AddExtensionWithActiveTab("ActiveTab Extension");
  auto enterprise_extension = AddExtensionWithActiveTab(
      "ActiveTab Extension",
      extensions::mojom::ManifestLocation::kExternalPolicy);

  int context_id = extensions::util::GetBrowserContextId(browser_context());
  extension->permissions_data()->SetContextId(context_id);
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  enterprise_extension->permissions_data()->SetContextId(context_id);
  enterprise_extension->permissions_data()->SetUsesDefaultHostRestrictions();

  // Add a policy-blocked site.
  URLPattern default_policy_blocked_pattern =
      URLPattern(URLPattern::SCHEME_ALL, "*://*.policy-blocked.com/*");
  extensions::URLPatternSet default_allowed_hosts;
  extensions::URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(default_policy_blocked_pattern);
  extensions::PermissionsData::SetDefaultPolicyHostRestrictions(
      context_id, default_blocked_hosts, default_allowed_hosts);

  // Allow enterprise extension access to policy-blocked site.
  extensions::URLPatternSet allowed_hosts;
  extensions::URLPatternSet blocked_hosts;
  allowed_hosts.AddPattern(default_policy_blocked_pattern);
  enterprise_extension->permissions_data()->SetPolicyHostRestrictions(
      blocked_hosts, allowed_hosts);

  // Verify only enterprise extension can have access with activeTab to
  // policy-blocked site.
  const GURL policy_url("http://www.policy-blocked.com");
  EXPECT_FALSE(manager_->HasActiveTabAndCanAccess(*extension, policy_url));
  EXPECT_TRUE(
      manager_->HasActiveTabAndCanAccess(*enterprise_extension, policy_url));
}

// Tests that HasRequestedHostPermissions returns true only for extensions
// that explicitly requested host permissions.
TEST_F(PermissionsManagerUnittest, HasRequestedHostPermissions) {
  auto no_permissions_extension = AddExtension("Extension");
  auto requested_site_extension = AddExtensionWithHostPermission(
      "RequestedUrl Extension", "*://*.requested.com/*");
  auto all_urls_extension = AddExtensionWithHostPermission(
      "RequestedUrl Extension", "*://*.requested.com/*");
  auto active_tab_extension = AddExtensionWithActiveTab("ActiveTab Extension");

  EXPECT_FALSE(
      manager_->HasRequestedHostPermissions(*no_permissions_extension));
  EXPECT_TRUE(manager_->HasRequestedHostPermissions(*requested_site_extension));
  EXPECT_TRUE(manager_->HasRequestedHostPermissions(*all_urls_extension));
  EXPECT_FALSE(manager_->HasRequestedHostPermissions(*active_tab_extension));
}

TEST_F(PermissionsManagerUnittest, HasRequestedActiveTab) {
  auto no_permissions_extension = AddExtension("Extension");
  auto requested_site_extension = AddExtensionWithHostPermission(
      "RequestedUrl Extension", "*://*.requested.com/*");
  auto dnr_extension =
      AddExtensionWithAPIPermission("DNR extension", "declarativeNetRequest");
  auto active_tab_extension = AddExtensionWithActiveTab("ActiveTab Extension");

  // Verify that HasRequestedActiveTab returns true only for extensions
  // that explicitly requested activeTab.
  EXPECT_FALSE(manager_->HasRequestedActiveTab(*no_permissions_extension));
  EXPECT_FALSE(manager_->HasRequestedActiveTab(*requested_site_extension));
  EXPECT_FALSE(manager_->HasRequestedActiveTab(*dnr_extension));
  EXPECT_TRUE(manager_->HasRequestedActiveTab(*active_tab_extension));
}

class PermissionsManagerWithPermittedSitesUnitTest
    : public PermissionsManagerUnittest {
 public:
  PermissionsManagerWithPermittedSitesUnitTest();
  PermissionsManagerWithPermittedSitesUnitTest(
      const PermissionsManagerWithPermittedSitesUnitTest&) = delete;
  const PermissionsManagerWithPermittedSitesUnitTest& operator=(
      const PermissionsManagerWithPermittedSitesUnitTest&) = delete;
  ~PermissionsManagerWithPermittedSitesUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

PermissionsManagerWithPermittedSitesUnitTest::
    PermissionsManagerWithPermittedSitesUnitTest() {
  feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
}

TEST_F(PermissionsManagerWithPermittedSitesUnitTest,
       AddAndRemovePermittedSite) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  const std::string expected_url_pattern = "http://a.example.com/*";
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);
  base::Value::List value_with_url;
  value_with_url.Append(url.Serialize());

  // Verify the permitted sites list is empty.
  EXPECT_EQ(GetPermittedSitesFromManager(), std::set<url::Origin>());
  EXPECT_EQ(GetPermittedSitesFromPrefs(), nullptr);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(), testing::IsEmpty());
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  manager_->AddUserPermittedSite(url);

  // Verify the site is stored both in manager and prefs permitted sites.
  EXPECT_EQ(GetPermittedSitesFromManager(), set_with_url);
  EXPECT_EQ(*GetPermittedSitesFromPrefs(), value_with_url);
  EXPECT_THAT(GetPermittedSitesFromPermissionsData(),
              testing::UnorderedElementsAre(expected_url_pattern));
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kGrantAllExtensions);

  // Adding an existent permitted site.
  manager_->AddUserPermittedSite(url);

  // Verify the entry is not duplicated.
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

TEST_F(PermissionsManagerWithPermittedSitesUnitTest, GrantAllExtensionsAccess) {
  const url::Origin url = url::Origin::Create(GURL("http://a.example.com"));
  std::set<url::Origin> empty_set;
  std::set<url::Origin> set_with_url;
  set_with_url.insert(url);

  manager_->UpdateUserSiteSetting(
      url, PermissionsManager::UserSiteSetting::kGrantAllExtensions);
  const PermissionsManager::UserPermissionsSettings& actual_permissions =
      manager_->GetUserPermissionsSettings();
  EXPECT_EQ(actual_permissions.restricted_sites, empty_set);
  EXPECT_EQ(actual_permissions.permitted_sites, set_with_url);
  EXPECT_EQ(manager_->GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kGrantAllExtensions);
}

TEST_F(PermissionsManagerWithPermittedSitesUnitTest,
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
