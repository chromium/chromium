// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_

#include "base/macros.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/mojom/host_id.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ContentVerifier;

// UserScriptLoader for extensions.
class ExtensionUserScriptLoader : public UserScriptLoader {
 public:
  struct PathAndLocaleInfo {
    base::FilePath file_path;
    std::string default_locale;
    extension_l10n_util::GzippedMessagesPermission gzip_permission;
  };

  // The listen_for_extension_system_loaded is only set true when initializing
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

  // Contains info needed for localization for this loader's host.
  PathAndLocaleInfo host_info_;

  // Manages content verification of the loaded user scripts.
  scoped_refptr<ContentVerifier> content_verifier_;

  base::WeakPtrFactory<ExtensionUserScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionUserScriptLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
