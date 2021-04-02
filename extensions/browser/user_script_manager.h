// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/web_ui_user_script_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class UserScriptLoader;

// Manages user scripts for all extensions and webview scripts from WebUI pages.
// Owns one UserScriptLoader for manifest extension scripts, and a map of
// mojom::HostID to UserScriptLoaders for declarative extension and WebUI
// scripts. File loading and shared memory management operations are delegated
// to these UserScriptLoaders.
class UserScriptManager : public ExtensionRegistryObserver {
 public:
  explicit UserScriptManager(content::BrowserContext* browser_context);
  ~UserScriptManager() override;
  UserScriptManager(const UserScriptManager& other) = delete;
  UserScriptManager& operator=(const UserScriptManager& other) = delete;

  UserScriptLoader* manifest_script_loader() {
    return &manifest_script_loader_;
  }

  UserScriptLoader* GetUserScriptLoaderByID(const mojom::HostID& host_id);

  ExtensionUserScriptLoader* GetUserScriptLoaderForExtension(
      const ExtensionId& extension_id);

  WebUIUserScriptLoader* GetUserScriptLoaderForWebUI(const GURL& url);

 private:
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

  // Creates a ExtensionUserScriptLoader object.
  // TODO(crbug.com/1168627): Remove this method once ExtensionUserScriptLoader
  // is created only when an extension loads.
  ExtensionUserScriptLoader* CreateExtensionUserScriptLoader(
      const ExtensionId& extension_id);

  // Creates a WebUIUserScriptLoader object.
  WebUIUserScriptLoader* CreateWebUIUserScriptLoader(const GURL& url);

  // Script loader for manifest extension scripts that handles loading contents
  // of scripts into shared memory and notifying renderers of scripts in shared
  // memory.
  ExtensionUserScriptLoader manifest_script_loader_;

  // A map of ExtensionUserScriptLoader for each extension host, with one loader
  // per extension. Currently, each loader is lazily initialized and contains
  // scripts from APIs webview tags.
  // TODO(crbug.com/1168627): Put manifest scripts in here too and remove
  // |manifest_script_loader_|.
  std::map<ExtensionId, std::unique_ptr<ExtensionUserScriptLoader>>
      extension_script_loaders_;

  // A map of WebUIUserScriptLoader for each WebUI host, each loader contains
  // webview content scripts for the corresponding WebUI page and is lazily
  // initialized.
  std::map<GURL, std::unique_ptr<WebUIUserScriptLoader>> webui_script_loaders_;

  content::BrowserContext* const browser_context_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
