// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ContentVerifier;
class StateStore;

// UserScriptLoader for extensions. To support the scripting API, user script
// ids/metadata registered from that API are also stored.
class ExtensionUserScriptLoader : public UserScriptLoader {
 public:
  using DynamicScriptsModifiedCallback =
      base::OnceCallback<void(const std::optional<std::string>& error)>;

  struct PathAndLocaleInfo {
    base::FilePath file_path;
    std::string default_locale;
    extension_l10n_util::GzippedMessagesPermission gzip_permission;
  };

  // The `listen_for_extension_system_loaded` is only set true when initializing
  // the Extension System, e.g, when constructs UserScriptManager in
  // ExtensionSystemImpl.
  ExtensionUserScriptLoader(content::BrowserContext* browser_context,
                            const Extension& extension,
                            StateStore* state_store,
                            bool listen_for_extension_system_loaded);
  ExtensionUserScriptLoader(content::BrowserContext* browser_context,
                            const Extension& extension,
                            StateStore* state_store,
                            bool listen_for_extension_system_loaded,
                            scoped_refptr<ContentVerifier> content_verifier);

  ExtensionUserScriptLoader(const ExtensionUserScriptLoader&) = delete;
  ExtensionUserScriptLoader& operator=(const ExtensionUserScriptLoader&) =
      delete;

  ~ExtensionUserScriptLoader() override;

  // Adds `script_ids` into `pending_dynamic_script_ids_` This is called before
  // the scripts the ids belong to are verified to ensure a later call
  // specifying the same script ids will be marked as a conflict.
  void AddPendingDynamicScriptIDs(std::set<std::string> script_ids);

  // Removes `script_ids` from `pending_dynamic_script_ids_`. Should only be
  // called when an API call is about to return with an error before attempting
  // to load its scripts.
  void RemovePendingDynamicScriptIDs(const std::set<std::string>& script_ids);

  // Adds manifest scripts from `extension` and calls GetDynamicScripts for
  // initial dynamic scripts, to the set of scripts managed by this loader. Once
  // `manifest_scripts` are loaded, calls `callback`. Returns whether any
  // scripts will be added for the initial load.
  bool AddScriptsForExtensionLoad(
      const Extension& extension,
      UserScriptLoader::ScriptsLoadedCallback callback);

  // Adds `scripts` to the set of scripts managed by this loader and once these
  // scripts are loaded, calls OnDynamicScriptsAdded, which also calls
  // `callback`.
  void AddDynamicScripts(UserScriptList scripts,
                         std::set<std::string> persistent_script_ids,
                         DynamicScriptsModifiedCallback callback);

  // Removes all dynamic scripts with an id specified in `ids` from
  // `pending_dynamic_script_ids_` and `loaded_dynamic_scripts_`.
  void RemoveDynamicScripts(const std::set<std::string>& ids_to_remove,
                            DynamicScriptsModifiedCallback callback);

  // Removes all dynamic scripts with `source` for the extension, including
  // loaded and pending scripts.
  void ClearDynamicScripts(UserScript::Source source,
                           DynamicScriptsModifiedCallback callback);

  // Updates `scripts` with `script_ids` from the set of scripts managed by this
  // loader, persisting the ones in `persistent_script_ids`. Invokes
  // `add_callback` once scripts are updated.
  void UpdateDynamicScripts(
      UserScriptList scripts,
      std::set<std::string> script_ids,
      std::set<std::string> persistent_script_ids,
      ExtensionUserScriptLoader::DynamicScriptsModifiedCallback add_callback);

  // Sets whether scripts with the given `source` should be enabled and
  // unloads / reloads any scripts with that source as appropriate.
  void SetSourceEnabled(UserScript::Source source, bool enabled);

  // Returns the IDs of all dynamic scripts with `source` for the extension,
  // which includes the IDs of all pending and loaded dynamic scripts.
  // Note: Some of these scripts may be inactive.
  std::set<std::string> GetDynamicScriptIDs(UserScript::Source source) const;

  // Returns the loaded dynamic scripts. Note: Some of these scripts may be
  // inactive.
  const UserScriptList& GetLoadedDynamicScripts() const;

  // Returns the IDs of all the currently-loaded persistent dynamic scripts for
  // the extension. Note that this does not include scripts currently in
  // `pending_dynamic_script_ids_`.
  std::set<std::string> GetPersistentDynamicScriptIDs() const;

  // A wrapper around the method to load user scripts. Waits for the user
  // scripts to load and returns the scripts that were loaded. Exposed only for
  // tests.
  UserScriptList LoadScriptsForTest(UserScriptList user_scripts);

 private:
  // A helper class which handles getting/setting script metadata from the
  // StateStore, and serializing/deserializing between base::Value and
  // UserScript.
  class DynamicScriptsStorageHelper {
   public:
    using DynamicScriptsReadCallback =
        base::OnceCallback<void(UserScriptList scripts)>;

    DynamicScriptsStorageHelper(content::BrowserContext* browser_context,
                                const ExtensionId& extension_id,
                                StateStore* state_store);
    ~DynamicScriptsStorageHelper();
    DynamicScriptsStorageHelper(const DynamicScriptsStorageHelper& other) =
        delete;
    DynamicScriptsStorageHelper& operator=(
        const DynamicScriptsStorageHelper& other) = delete;

    // Retrieves dynamic scripts from the StateStore. Calls
    // OnDynamicScriptsReadFromStorage when done, which then calls `callback`.
    void GetDynamicScripts(DynamicScriptsReadCallback callback);

    // Persists the metadata of the current set of loaded dynamic scripts into
    // the StateStore.
    void SetDynamicScripts(
        const UserScriptList& scripts,
        const std::set<std::string>& persistent_dynamic_script_ids);

   private:
    // Called when dynamic scripts have been retrieved from the StateStore.
    // Deserializes `value` into a UserScriptList and calls `callback` with that
    // list.
    void OnDynamicScriptsReadFromStorage(DynamicScriptsReadCallback callback,
                                         std::optional<base::Value> value);

    raw_ptr<content::BrowserContext> browser_context_;

    ExtensionId extension_id_;

    // A non-owning pointer to the StateStore which contains metadata of
    // persistent dynamic scripts owned by this extension. Outlives this
    // instance and the owning ExtensionUserScriptLoader.
    raw_ptr<StateStore> state_store_;

    base::WeakPtrFactory<DynamicScriptsStorageHelper> weak_factory_{this};
  };

  // UserScriptLoader:
  void LoadScripts(UserScriptList user_scripts,
                   const std::set<std::string>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Initiates script load when we have been waiting for the extension system
  // to be ready.
  void OnExtensionSystemReady();

  // Called when the extension's initial set of persistent dynamic scripts have
  // been fetched right after the extension has been loaded.
  void OnInitialDynamicScriptsReadFromStateStore(
      UserScriptList scripts,
      UserScriptLoader::ScriptsLoadedCallback callback,
      UserScriptList initial_dynamic_scripts);

  // Called when the extension's initial set of dynamic scripts have been
  // loaded.
  void OnInitialExtensionScriptsLoaded(UserScriptList initial_dynamic_scripts,
                                       ScriptsLoadedCallback callback,
                                       UserScriptLoader* loader,
                                       const std::optional<std::string>& error);

  // Called when the scripts added by AddDynamicScripts have been loaded. Since
  // `added_scripts` corresponds to newly loaded scripts, their IDs are removed
  // from `pending_dynamic_script_ids_` and their metadata added to
  // `loaded_dynamic_scripts_`.
  void OnDynamicScriptsAdded(UserScriptList added_scripts,
                             std::set<std::string> new_persistent_script_ids,
                             DynamicScriptsModifiedCallback callback,
                             UserScriptLoader* loader,
                             const std::optional<std::string>& error);

  // Called when the scripts to be removed in RemoveDynamicScripts are removed.
  // All scripts in `loaded_dynamic_scripts_` with their id in
  // `removed_script_ids` are removed.
  void OnDynamicScriptsRemoved(const std::set<std::string>& removed_script_ids,
                               DynamicScriptsModifiedCallback callback,
                               UserScriptLoader* loader,
                               const std::optional<std::string>& error);

  // Checks if the extension has initial dynamic scripts by checking if the
  // extension has the scripting or user scripts permission, and if URLPatterns
  // from dynamic scripts are registered in prefs.
  bool HasInitialDynamicScripts(const Extension& extension) const;

  // The IDs of dynamically registered scripts (e.g. registered by the
  // extension's API calls) that have not been loaded yet. IDs are removed from
  // the set when:
  //  - Their corresponding scripts have been loaded.
  //  - A load for the IDs has failed.
  //  - A load for the IDs will no longer be initiated.
  //  - An unregisterContentScripts call was made for one or more ids in this
  //    set.
  std::set<std::string> pending_dynamic_script_ids_;

  // The metadata of dynamic scripts from the extension that have been loaded.
  // Note: some of these scripts may be disabled; see `disabled_sources_`.
  UserScriptList loaded_dynamic_scripts_;

  // The set of sources to disallow. Scripts with these sources will still be
  // loaded in this class (in `loaded_dynamic_scripts_`) so that they are still
  // properly stored and persisted when re-writing the database; however, they
  // are not added to any renderers or injected.
  std::set<UserScript::Source> disabled_sources_;

  // The IDs of loaded dynamic scripts that persist across sessions.
  std::set<std::string> persistent_dynamic_script_ids_;

  // Contains info needed for localization for this loader's host.
  PathAndLocaleInfo host_info_;

  DynamicScriptsStorageHelper helper_;

  // Manages content verification of the loaded user scripts.
  scoped_refptr<ContentVerifier> content_verifier_;

  base::WeakPtrFactory<ExtensionUserScriptLoader> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
