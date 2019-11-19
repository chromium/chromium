// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_

#include "base/macros.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ContentVerifier;

// UserScriptLoader for extensions.
class ExtensionUserScriptLoader : public UserScriptLoader,
                                  public ExtensionRegistryObserver {
 public:
  using PathAndDefaultLocale = std::pair<base::FilePath, std::string>;
  using HostsInfo = std::map<HostID, PathAndDefaultLocale>;

  // The listen_for_extension_system_loaded is only set true when initilizing
  // the Extension System, e.g, when constructs SharedUserScriptMaster in
  // ExtensionSystemImpl.
  ExtensionUserScriptLoader(content::BrowserContext* browser_context,
                            const HostID& host_id,
                            bool listen_for_extension_system_loaded);
  ~ExtensionUserScriptLoader() override;

  // A wrapper around the method to load user scripts, which is normally run on
  // the file thread. Exposed only for tests.
  void LoadScriptsForTest(UserScriptList* user_scripts);

 private:
  // UserScriptLoader:
  void LoadScripts(std::unique_ptr<UserScriptList> user_scripts,
                   const std::set<HostID>& changed_hosts,
                   const std::set<int>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Updates |hosts_info_| to contain info for each element of
  //  |changed_hosts_|.
  void UpdateHostsInfo(const std::set<HostID>& changed_hosts);

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Initiates script load when we have been waiting for the extension system
  // to be ready.
  void OnExtensionSystemReady();

  // Maps host info needed for localization to a host ID.
  HostsInfo hosts_info_;

  // Manages content verification of the loaded user scripts.
  scoped_refptr<ContentVerifier> content_verifier_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<ExtensionUserScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionUserScriptLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
