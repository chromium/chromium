// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_

#include <set>

#include "base/macros.h"
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

// UserScriptLoader for extensions. To support the scripting API, user script
// ids/metadata registered from that API are also stored.
class ExtensionUserScriptLoader : public UserScriptLoader {
 public:
  using DynamicScriptsModifiedCallback =
      base::OnceCallback<void(const absl::optional<std::string>& error)>;

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
                            bool listen_for_extension_system_loaded);
  ExtensionUserScriptLoader(content::BrowserContext* browser_context,
                            const Extension& extension,
                            bool listen_for_extension_system_loaded,
                            scoped_refptr<ContentVerifier> content_verifier);
  ~ExtensionUserScriptLoader() override;

  // Adds `script_ids` into `pending_dynamic_script_ids_` This is called before
  // the scripts the ids belong to are verified to ensure a later call
  // specifying the same script ids will be marked as a conflict.
  void AddPendingDynamicScriptIDs(std::set<std::string> script_ids);

  // Removes `script_ids` from `pending_dynamic_script_ids_`. Should only be
  // called when an API call is about to return with an error before attempting
  // to load its scripts.
  void RemovePendingDynamicScriptIDs(const std::set<std::string>& script_ids);

  // Adds `scripts` to the set of scripts managed by this loader and once these
  // scripts are loaded, calls OnDynamicScriptsAdded, which also calls
  // `callback`.
  void AddDynamicScripts(std::unique_ptr<UserScriptList> scripts,
                         DynamicScriptsModifiedCallback callback);

  // Returns the IDs of all dynamic scripts for the extension, which includes
  // the IDs of all pending and loaded dynamic scripts.
  std::set<std::string> GetDynamicScriptIDs();

  const UserScriptList& GetLoadedDynamicScripts();

  // A wrapper around the method to load user scripts. Waits for the user
  // scripts to load and returns the scripts that were loaded. Exposed only for
  // tests.
  std::unique_ptr<UserScriptList> LoadScriptsForTest(
      std::unique_ptr<UserScriptList> user_scripts);

 private:
  // UserScriptLoader:
  void LoadScripts(std::unique_ptr<UserScriptList> user_scripts,
                   const std::set<std::string>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Initiates script load when we have been waiting for the extension system
  // to be ready.
  void OnExtensionSystemReady();

  // Called when the scripts added by AddDynamicScripts have been loaded. Since
  // `added_scripts` corresponds to newly loaded scripts, their IDs are removed
  // from `pending_dynamic_script_ids_` and their metadata added to
  // `loaded_dynamic_scripts_`.
  void OnDynamicScriptsAdded(std::unique_ptr<UserScriptList> added_scripts,
                             DynamicScriptsModifiedCallback callback,
                             UserScriptLoader* loader,
                             const absl::optional<std::string>& error);

  // The IDs of dynamically registered scripts (e.g. registered by the
  // extension's API calls) that have not been loaded yet. IDs are removed from
  // the set when:
  //  - Their corresponding scripts have been loaded.
  //  - A load for the IDs has failed.
  //  - A load for the IDs will no longer be initiated.
  std::set<std::string> pending_dynamic_script_ids_;

  // The metadata of dynamic scripts from the extension that have been loaded.
  UserScriptList loaded_dynamic_scripts_;

  // Contains info needed for localization for this loader's host.
  PathAndLocaleInfo host_info_;

  // Manages content verification of the loaded user scripts.
  scoped_refptr<ContentVerifier> content_verifier_;

  base::WeakPtrFactory<ExtensionUserScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionUserScriptLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
