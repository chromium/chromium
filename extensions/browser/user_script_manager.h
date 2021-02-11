// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/scoped_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class DeclarativeUserScriptSet;

// Manages user scripts for all extensions and webview scripts from WebUI pages.
// Owns one UserScriptLoader for manifest extension scripts, and a map of HostID
// to UserScriptLoaders for declarative extension and WebUI scripts. File
// loading and shared memory management operations are delegated to these
// UserScriptLoaders.
class UserScriptManager : public ExtensionRegistryObserver {
 public:
  explicit UserScriptManager(content::BrowserContext* browser_context);
  ~UserScriptManager() override;
  UserScriptManager(const UserScriptManager& other) = delete;
  UserScriptManager& operator=(const UserScriptManager& other) = delete;

  UserScriptLoader* manifest_script_loader() {
    return &manifest_script_loader_;
  }

  // Gets the user script set for declarative scripts by the given HostID.
  // If one does not exist, a new object will be created.
  DeclarativeUserScriptSet* GetDeclarativeUserScriptSetByID(
      const HostID& host_id);

 private:
  using UserScriptSetMap =
      std::map<HostID, std::unique_ptr<DeclarativeUserScriptSet>>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Gets an extension's manifest scripts' metadata; i.e., gets a list of
  // UserScript objects that contains script info, but not the contents of the
  // scripts.
  std::unique_ptr<UserScriptList> GetManifestScriptsMetadata(
      const Extension* extension);

  // Creates a DeclarativeUserScriptSet object.
  DeclarativeUserScriptSet* CreateDeclarativeUserScriptSet(
      const HostID& host_id);

  // A map of DeclarativeUserScriptSets for each host. Each set is lazily
  // initialized, and contains scripts from APIs for an extension host, or
  // webview scripts for a WebUI host.
  // TODO(crbug.com/1168627): Have one UserScriptLoader per extension, and split
  // WebUI loaders into another set.
  UserScriptSetMap declarative_user_script_sets_;

  // Script loader for manifest extension scripts that handles loading contents
  // of scripts into shared memory and notifying renderers of scripts in shared
  // memory.
  ExtensionUserScriptLoader manifest_script_loader_;

  content::BrowserContext* const browser_context_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
