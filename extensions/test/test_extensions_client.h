// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extensions_client.h"
#include "url/gurl.h"

namespace extensions {

class TestExtensionsClient : public ExtensionsClient {
 public:
  // An interface that lets tests change the set of image paths before they are
  // returned by TestExtensionClient::GetBrowserImagePaths.
  class BrowserImagePathsFilter {
   public:
    virtual void Filter(const Extension* extension,
                        std::set<base::FilePath>* paths) = 0;
  };

  TestExtensionsClient();
  TestExtensionsClient(const TestExtensionsClient&) = delete;
  TestExtensionsClient& operator=(const TestExtensionsClient&) = delete;
  ~TestExtensionsClient() override;

  void AddBrowserImagePathsFilter(BrowserImagePathsFilter* filter);
  void RemoveBrowserImagePathsFilter(BrowserImagePathsFilter* filter);

 private:
  void Initialize() override;
  void InitializeWebStoreUrls(base::CommandLine* command_line) override;
  const PermissionMessageProvider& GetPermissionMessageProvider()
      const override;
  const std::string GetProductName() override;
  void FilterHostPermissions(const URLPatternSet& hosts,
                             URLPatternSet* new_hosts,
                             PermissionIDSet* permissions) const override;
  void SetScriptingAllowlist(const ScriptingAllowlist& allowlist) override;
  const ScriptingAllowlist& GetScriptingAllowlist() const override;
  URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const override;
  bool IsScriptableURL(const GURL& url, std::string* error) const override;
  const GURL& GetWebstoreBaseURL() const override;
  const GURL& GetNewWebstoreBaseURL() const override;
  const GURL& GetWebstoreUpdateURL() const override;
  bool IsBlocklistUpdateURL(const GURL& url) const override;
  std::set<base::FilePath> GetBrowserImagePaths(
      const Extension* extension) override;

  // A allowlist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  ScriptingAllowlist scripting_allowlist_;

  std::set<raw_ptr<BrowserImagePathsFilter, SetExperimental>>
      browser_image_filters_;

  const GURL webstore_base_url_;
  const GURL new_webstore_base_url_;
  GURL webstore_update_url_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_
