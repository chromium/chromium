// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EMBEDDER_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EMBEDDER_USER_SCRIPT_LOADER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "extensions/browser/url_fetcher.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/mojom/host_id.mojom.h"

namespace content {
class BrowserContext;
}

// UserScriptLoader for embedders, such as WebUI and Controlled Frame.
class EmbedderUserScriptLoader : public extensions::UserScriptLoader {
 public:
  EmbedderUserScriptLoader(content::BrowserContext* browser_context,
                           const extensions::mojom::HostID& host_id);

  EmbedderUserScriptLoader(const EmbedderUserScriptLoader&) = delete;
  EmbedderUserScriptLoader& operator=(const EmbedderUserScriptLoader&) = delete;

  ~EmbedderUserScriptLoader() override;

 private:
  struct UserScriptRenderInfo;
  using UserScriptRenderInfoMap = std::map<std::string, UserScriptRenderInfo>;

  // UserScriptLoader:
  void AddScripts(extensions::UserScriptList scripts,
                  int render_process_id,
                  int render_frame_id,
                  ScriptsLoadedCallback callback) override;
  void LoadScripts(extensions::UserScriptList user_scripts,
                   const std::set<std::string>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Called at the end of each fetch, tracking whether all fetches are done.
  void OnSingleEmbedderURLFetchComplete(
      extensions::UserScript::Content* script_file,
      bool success,
      std::unique_ptr<std::string> data);

  // Called when the loads of the user scripts are done.
  void OnEmbedderURLFetchComplete();

  // Creates WebUiURLFetchers for the given `contents`.
  void CreateEmbedderURLFetchers(
      const extensions::UserScript::ContentList& contents,
      int render_process_id,
      int render_frame_id);

  // Caches the render info of script from embedders when AddScripts is called.
  // When starting to load the script, we look up this map to retrieve the
  // render info. It is used for the script from embedders only, since the fetch
  // of script content requires the info of associated render.
  UserScriptRenderInfoMap script_render_info_map_;

  // The number of complete fetchs.
  size_t complete_fetchers_;

  // Caches |user_scripts_| from UserScriptLoader when loading.
  extensions::UserScriptList user_scripts_cache_;

  LoadScriptsCallback scripts_loaded_callback_;

  std::vector<std::unique_ptr<extensions::URLFetcher>> fetchers_;

  base::WeakPtrFactory<EmbedderUserScriptLoader> weak_ptr_factory_{this};
};

#endif  // EXTENSIONS_BROWSER_EMBEDDER_USER_SCRIPT_LOADER_H_
