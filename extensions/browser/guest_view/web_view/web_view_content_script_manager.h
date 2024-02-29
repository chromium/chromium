// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {

class UserScriptLoader;

// WebViewContentScriptManager manages the content scripts that each webview
// guest adds and removes programmatically.
class WebViewContentScriptManager : public base::SupportsUserData::Data {
 public:
  explicit WebViewContentScriptManager(
      content::BrowserContext* browser_context);

  WebViewContentScriptManager(const WebViewContentScriptManager&) = delete;
  WebViewContentScriptManager& operator=(const WebViewContentScriptManager&) =
      delete;

  ~WebViewContentScriptManager() override;

  static WebViewContentScriptManager* Get(
      content::BrowserContext* browser_context);

  // Adds content scripts for the WebView specified by
  // |embedder_process_id| and |view_instance_id|.
  void AddContentScripts(int embedder_process_id,
                         content::RenderFrameHost* render_frame_host,
                         int view_instance_id,
                         const mojom::HostID& host_id,
                         UserScriptList user_scripts);

  // Removes all content scripts for the WebView identified by
  // |embedder_process_id| and |view_instance_id|.
  void RemoveAllContentScriptsForWebView(int embedder_process_id,
                                         int view_instance_id);

  // Removes contents scipts whose names are in the |script_name_list| for the
  // WebView specified by |embedder_process_id| and |view_instance_id|.
  // If the |script_name_list| is empty, removes all the content scripts added
  // for this WebView.
  void RemoveContentScripts(int embedder_process_id,
                            int view_instance_id,
                            const mojom::HostID& host_id,
                            const std::vector<std::string>& script_name_list);

  // Returns the content script IDs added by the WebView specified by
  // |embedder_process_id| and |view_instance_id|.
  std::set<std::string> GetContentScriptIDSet(int embedder_process_id,
                                              int view_instance_id);

  // Checks if there is any pending content script updates.
  // If not, run |callback| immediately; otherwise caches the |callback|, and
  // the |callback| will be called after all the pending content scripts are
  // loaded.
  void SignalOnScriptsUpdated(base::OnceClosure callback);

 private:
  using GuestMapKey = std::pair<int, int>;

  // Maps a content script's name to its id.
  using ContentScriptMap = std::map<std::string, std::string>;
  using GuestContentScriptMap = std::map<GuestMapKey, ContentScriptMap>;

  // Invoked when scripts are updated from any kind of operation or when a
  // UserScriptLoader is about to be destroyed. This may be called multiple
  // times per script load.
  void OnScriptsUpdated(UserScriptLoader* loader,
                        const std::optional<std::string>& error);

  // If there are no pending script loads, we will run all the remaining
  // callbacks in |pending_scripts_loading_callbacks_|.
  void RunCallbacksIfReady();

  // A map from embedder process ID and view instance ID (uniquely identifying
  // one webview) to that webview's host ID. All webviews that have content
  // scripts registered through this WebViewContentScriptManager will have an
  // entry in this map.
  std::map<GuestMapKey, mojom::HostID> webview_host_id_map_;

  GuestContentScriptMap guest_content_script_map_;

  // Tracks the number of pending Add/Remove content script operations initiated
  // from this class.
  int pending_operation_count_ = 0;

  // Caches callbacks and resumes them when all the scripts are loaded.
  std::vector<base::OnceClosure> pending_scripts_loading_callbacks_;

  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<WebViewContentScriptManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_
