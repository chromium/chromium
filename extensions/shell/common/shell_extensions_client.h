// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_

#include "extensions/common/extensions_client.h"
#include "url/gurl.h"

namespace extensions {

// The app_shell implementation of ExtensionsClient.
class ShellExtensionsClient : public ExtensionsClient {
 public:
  ShellExtensionsClient();
  ShellExtensionsClient(const ShellExtensionsClient&) = delete;
  ShellExtensionsClient& operator=(const ShellExtensionsClient&) = delete;
  ~ShellExtensionsClient() override;

  // ExtensionsClient overrides:
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

 private:
  ScriptingAllowlist scripting_allowlist_;

  const GURL webstore_base_url_;
  const GURL new_webstore_base_url_;
  const GURL webstore_update_url_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_
