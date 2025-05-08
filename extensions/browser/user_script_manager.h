// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/embedder_user_script_loader.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class UserScriptLoader;

// Manages user scripts for all extensions and webview scripts from embedder
// pages. Owns one UserScriptLoader for manifest extension scripts, and a map
// of mojom::HostID to UserScriptLoaders for declarative extension and embedder
// scripts. File loading and shared memory management operations are delegated
// to these UserScriptLoaders.
class UserScriptManager : public ExtensionRegistryObserver {
 public:
  explicit UserScriptManager(content::BrowserContext* browser_context);
  ~UserScriptManager() override;
  UserScriptManager(const UserScriptManager& other) = delete;
  UserScriptManager& operator=(const UserScriptManager& other) = delete;

  // Key corresponding to whether the user has allowed user scripts to run for
  // the extension.
  static constexpr PrefMap kUserScriptsAllowedPref = {
      "user_scripts_enabled", PrefType::kBool, PrefScope::kExtensionSpecific};
  // Key corresponding to whether the migration from using the dev mode toggle
  // to the per-extension toggle for enabling dynamic user scripts usage has
  // completed.
  static constexpr PrefMap kUserScriptsToggleMigratedPref = {
      "migrated_user_scripts_toggle", PrefType::kBool, PrefScope::kProfile};

  UserScriptLoader* GetUserScriptLoaderByID(const mojom::HostID& host_id);

  ExtensionUserScriptLoader* GetUserScriptLoaderForExtension(
      const ExtensionId& extension_id);

  EmbedderUserScriptLoader* GetUserScriptLoaderForEmbedder(
      const mojom::HostID& host_id);

  // Sets whether scripts of the given `source` should be enabled for
  // (all) extensions. Does not affect embedder script loaders.
  void SetUserScriptSourceEnabledForExtensions(UserScript::Source source,
                                               bool enabled);

  // Returns true if the extension is allowed to use the userScripts API.
  // Note: this may also seed feature availability state the first time it is
  // called so that it is always accurate.
  bool AreUserScriptsAllowed(const Extension& extension);

  // Returns whether the extension has permission to run user scripts or can
  // request permission to do so.
  static bool IsUserScriptsAPIPermissionAvailable(const Extension& extension);

  bool IsUserScriptPrefEnabledForTesting(
      const ExtensionId& extension_id) const {
    return IsUserScriptPrefEnabled(extension_id);
  }

  // Set extension preference for userScripts API being allowed.
  void SetUserScriptPrefEnabled(const ExtensionId& extension_id, bool enabled);

 private:
  // ExtensionRegistryObserver implementation.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Called when `loader` has finished loading its initial set of scripts. This
  // is only fired for extension script loaders.
  void OnInitialExtensionLoadComplete(UserScriptLoader* loader,
                                      const std::optional<std::string>& error);

  // Removes the given ID from `pending_initial_extension_loads_` and if there
  // are no more pending initial loads, signal to the UserScriptListener.
  void RemovePendingExtensionLoadAndSignal(const ExtensionId& extension_id);

  // Creates a ExtensionUserScriptLoader object.
  ExtensionUserScriptLoader* CreateExtensionUserScriptLoader(
      const Extension* extension);

  // Creates a EmbedderUserScriptLoader object.
  EmbedderUserScriptLoader* CreateEmbedderUserScriptLoader(
      const mojom::HostID& host_id);

  // Migrate an extension from dev mode toggle to per-extension toggle if not
  // done, otherwise just set the allowed state from the current allowed
  // preference.
  void InitializeUserScriptState(const Extension& extension);

  // Get extension preference for userScripts API being allowed.
  bool IsUserScriptPrefEnabled(const ExtensionId& extension_id) const;

  // Migrates an eligible extension to use the per-extension toggle.
  void MigrateUserScriptExtension(const Extension& extension);

  // Migrates all non-enabled extensions to use the per-extension toggle.
  void MigrateUserScriptExtensions();

  // A map of ExtensionUserScriptLoader for each extension host, with one loader
  // per extension. Currently, each loader is lazily initialized and contains
  // scripts from APIs webview tags.
  std::map<ExtensionId, std::unique_ptr<ExtensionUserScriptLoader>>
      extension_script_loaders_;

  // A map of EmbedderUserScriptLoader for each embedder host, each loader
  // contains webview content scripts for the corresponding embedder page and is
  // lazily initialized.
  std::map<mojom::HostID, std::unique_ptr<EmbedderUserScriptLoader>>
      embedder_script_loaders_;

  // Tracks the IDs of extensions with initial script loads (consisting of
  // manifest and persistent dynamic scripts) in progress.
  std::set<ExtensionId> pending_initial_extension_loads_;

  const raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<UserScriptManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_USER_SCRIPT_MANAGER_H_
