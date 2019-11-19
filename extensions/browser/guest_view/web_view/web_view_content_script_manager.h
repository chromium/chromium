// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "extensions/browser/user_script_loader.h"

struct HostID;

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {

// WebViewContentScriptManager manages the content scripts that each webview
// guest adds and removes programmatically.
class WebViewContentScriptManager : public base::SupportsUserData::Data,
                                    public UserScriptLoader::Observer {
 public:
  explicit WebViewContentScriptManager(
      content::BrowserContext* browser_context);
  ~WebViewContentScriptManager() override;

  static WebViewContentScriptManager* Get(
      content::BrowserContext* browser_context);

  // Adds content scripts for the WebView specified by
  // |embedder_process_id| and |view_instance_id|.
  void AddContentScripts(int embedder_process_id,
                         content::RenderFrameHost* render_frame_host,
                         int view_instance_id,
                         const HostID& host_id,
                         std::unique_ptr<UserScriptList> user_scripts);

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
                            const HostID& host_id,
                            const std::vector<std::string>& script_name_list);

  // Returns the content script IDs added by the WebView specified by
  // |embedder_process_id| and |view_instance_id|.
  std::set<int> GetContentScriptIDSet(int embedder_process_id,
                                      int view_instance_id);

  // Checks if there is any pending content scripts to load.
  // If no, run |callback| immediately; otherwise caches the |callback|, and
  // the |callback| will be called after all the pending content scripts are
  // loaded.
  void SignalOnScriptsLoaded(base::OnceClosure callback);

 private:
  using GuestMapKey = std::pair<int, int>;
  using ContentScriptMap = std::map<std::string, UserScriptIDPair>;
  using GuestContentScriptMap = std::map<GuestMapKey, ContentScriptMap>;

  // UserScriptLoader::Observer implementation:
  void OnScriptsLoaded(UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override;
  void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) override;

  // If |user_script_loader_observer_| doesn't observe any source, we will run
  // all the remaining callbacks in |pending_scripts_loading_callbacks_|.
  void RunCallbacksIfReady();

  // A map from embedder process ID and view instance ID (uniquely identifying
  // one webview) to that webview's host ID. All webviews that have content
  // scripts registered through this WebViewContentScriptManager will have an
  // entry in this map.
  std::map<GuestMapKey, HostID> webview_host_id_map_;

  GuestContentScriptMap guest_content_script_map_;

  // WebViewContentScriptManager observes UserScriptLoader to wait for scripts
  // loaded event.
  ScopedObserver<UserScriptLoader, UserScriptLoader::Observer>
      user_script_loader_observer_;

  // Caches callbacks and resumes them when all the scripts are loaded.
  std::vector<base::OnceClosure> pending_scripts_loading_callbacks_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebViewContentScriptManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_CONTENT_SCRIPT_MANAGER_H_
