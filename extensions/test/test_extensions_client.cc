// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_extensions_client.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/grit/extensions_resources.h"
#include "extensions/test/test_permission_message_provider.h"

namespace extensions {

TestExtensionsClient::TestExtensionsClient()
    : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
      new_webstore_base_url_(extension_urls::kNewChromeWebstoreBaseURL),
      webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
}

TestExtensionsClient::~TestExtensionsClient() {
}

void TestExtensionsClient::AddBrowserImagePathsFilter(
    BrowserImagePathsFilter* filter) {
  browser_image_filters_.insert(filter);
}

void TestExtensionsClient::RemoveBrowserImagePathsFilter(
    BrowserImagePathsFilter* filter) {
  browser_image_filters_.erase(filter);
}

void TestExtensionsClient::Initialize() {
}

void TestExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {
  // TODO (mxnguyen): Move |kAppsGalleryUpdateURL| constant from chrome/... to
  // extensions/... to avoid referring to the constant value directly.
  if (command_line->HasSwitch("apps-gallery-update-url")) {
    webstore_update_url_ =
        GURL(command_line->GetSwitchValueASCII("apps-gallery-update-url"));
  }
}

const PermissionMessageProvider&
TestExtensionsClient::GetPermissionMessageProvider() const {
  static TestPermissionMessageProvider provider;
  return provider;
}

const std::string TestExtensionsClient::GetProductName() {
  return "extensions_test";
}

void TestExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
}

void TestExtensionsClient::SetScriptingAllowlist(
    const ExtensionsClient::ScriptingAllowlist& allowlist) {
  scripting_allowlist_ = allowlist;
}

const ExtensionsClient::ScriptingAllowlist&
TestExtensionsClient::GetScriptingAllowlist() const {
  return scripting_allowlist_;
}

URLPatternSet TestExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  URLPatternSet hosts;
  return hosts;
}

bool TestExtensionsClient::IsScriptableURL(const GURL& url,
                                           std::string* error) const {
  return true;
}

const GURL& TestExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& TestExtensionsClient::GetNewWebstoreBaseURL() const {
  return new_webstore_base_url_;
}

const GURL& TestExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool TestExtensionsClient::IsBlocklistUpdateURL(const GURL& url) const {
  return false;
}

std::set<base::FilePath> TestExtensionsClient::GetBrowserImagePaths(
    const Extension* extension) {
  std::set<base::FilePath> result =
      ExtensionsClient::GetBrowserImagePaths(extension);
  for (BrowserImagePathsFilter* filter : browser_image_filters_) {
    filter->Filter(extension, &result);
  }
  return result;
}

}  // namespace extensions
