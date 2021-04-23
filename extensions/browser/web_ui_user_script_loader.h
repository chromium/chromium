// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"

class GURL;
class WebUIURLFetcher;

namespace content {
class BrowserContext;
}

// UserScriptLoader for WebUI.
class WebUIUserScriptLoader : public extensions::UserScriptLoader {
 public:
  WebUIUserScriptLoader(content::BrowserContext* browser_context,
                        const GURL& url);
  ~WebUIUserScriptLoader() override;

 private:
  struct UserScriptRenderInfo;
  using UserScriptRenderInfoMap = std::map<std::string, UserScriptRenderInfo>;

  // UserScriptLoader:
  void AddScripts(std::unique_ptr<extensions::UserScriptList> scripts,
                  int render_process_id,
                  int render_frame_id,
                  ScriptsLoadedCallback callback) override;
  void LoadScripts(std::unique_ptr<extensions::UserScriptList> user_scripts,
                   const std::set<std::string>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Called at the end of each fetch, tracking whether all fetches are done.
  void OnSingleWebUIURLFetchComplete(extensions::UserScript::File* script_file,
                                     bool success,
                                     std::unique_ptr<std::string> data);

  // Called when the loads of the user scripts are done.
  void OnWebUIURLFetchComplete();

  // Creates WebUiURLFetchers for the given |script_files|.
  void CreateWebUIURLFetchers(
      const extensions::UserScript::FileList& script_files,
      int render_process_id,
      int render_frame_id);

  // Caches the render info of script from WebUI when AddScripts is called.
  // When starting to load the script, we look up this map to retrieve the
  // render info. It is used for the script from WebUI only, since the fetch
  // of script content requires the info of associated render.
  UserScriptRenderInfoMap script_render_info_map_;

  // The number of complete fetchs.
  size_t complete_fetchers_;

  // Caches |user_scripts_| from UserScriptLoader when loading.
  std::unique_ptr<extensions::UserScriptList> user_scripts_cache_;

  LoadScriptsCallback scripts_loaded_callback_;

  std::vector<std::unique_ptr<WebUIURLFetcher>> fetchers_;

  DISALLOW_COPY_AND_ASSIGN(WebUIUserScriptLoader);
};

#endif  // EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_
