// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/declarative_user_script_manager.h"
#include "extensions/browser/declarative_user_script_master.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"

using content::BrowserThread;

namespace extensions {

WebViewContentScriptManager::WebViewContentScriptManager(
    content::BrowserContext* browser_context)
    : user_script_loader_observer_(this), browser_context_(browser_context)  {
}

WebViewContentScriptManager::~WebViewContentScriptManager() {
}

WebViewContentScriptManager* WebViewContentScriptManager::Get(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebViewContentScriptManager* manager =
      static_cast<WebViewContentScriptManager*>(browser_context->GetUserData(
          webview::kWebViewContentScriptManagerKeyName));
  if (!manager) {
    manager = new WebViewContentScriptManager(browser_context);
    browser_context->SetUserData(webview::kWebViewContentScriptManagerKeyName,
                                 base::WrapUnique(manager));
  }
  return manager;
}

void WebViewContentScriptManager::AddContentScripts(
    int embedder_process_id,
    content::RenderFrameHost* render_frame_host,
    int view_instance_id,
    const HostID& host_id,
    std::unique_ptr<UserScriptList> scripts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DeclarativeUserScriptMaster* master =
      DeclarativeUserScriptManager::Get(browser_context_)
          ->GetDeclarativeUserScriptMasterByID(host_id);
  DCHECK(master);

  // We need to update WebViewRenderState.
  std::set<int> ids_to_add;

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  auto iter = guest_content_script_map_.find(key);

  // Step 1: finds the entry in guest_content_script_map_ by the given |key|.
  // If there isn't any content script added for the given guest yet, insert an
  // empty map first.
  if (iter == guest_content_script_map_.end()) {
    iter = guest_content_script_map_.insert(
        iter,
        std::pair<GuestMapKey, ContentScriptMap>(key, ContentScriptMap()));
  }

  // Step 2: updates the guest_content_script_map_.
  ContentScriptMap& map = iter->second;
  std::set<UserScriptIDPair> to_delete;
  for (const std::unique_ptr<UserScript>& script : *scripts) {
    auto map_iter = map.find(script->name());
    // If a content script has the same name as the new one, remove the old
    // script first, and insert the new one.
    if (map_iter != map.end()) {
      to_delete.insert(map_iter->second);
      map.erase(map_iter);
    }
    map.insert(std::pair<std::string, UserScriptIDPair>(
        script->name(), UserScriptIDPair(script->id(), script->host_id())));
    ids_to_add.insert(script->id());
  }

  if (!to_delete.empty())
    master->RemoveScripts(to_delete);

  // Step 3: makes WebViewContentScriptManager become an observer of the
  // |loader| for scripts loaded event.
  UserScriptLoader* loader = master->loader();
  DCHECK(loader);
  if (!user_script_loader_observer_.IsObserving(loader))
    user_script_loader_observer_.Add(loader);

  // Step 4: adds new scripts to the master.
  master->AddScripts(std::move(scripts), embedder_process_id,
                     render_frame_host->GetRoutingID());

  // Step 5: creates an entry in |webview_host_id_map_| for the given
  // |embedder_process_id| and |view_instance_id| if it doesn't exist.
  auto host_it = webview_host_id_map_.find(key);
  if (host_it == webview_host_id_map_.end())
    webview_host_id_map_.insert(std::make_pair(key, host_id));

  // Step 6: updates WebViewRenderState.
  if (!ids_to_add.empty()) {
    WebViewRendererState::GetInstance()->AddContentScriptIDs(
        embedder_process_id, view_instance_id, ids_to_add);
  }
}

void WebViewContentScriptManager::RemoveAllContentScriptsForWebView(
    int embedder_process_id,
    int view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Look up the host ID for the WebView.
  GuestMapKey key = std::make_pair(embedder_process_id, view_instance_id);
  auto host_it = webview_host_id_map_.find(key);
  // If no entry exists, then this WebView has no content scripts.
  if (host_it == webview_host_id_map_.end())
    return;

  // Remove all content scripts for the WebView.
  RemoveContentScripts(embedder_process_id, view_instance_id, host_it->second,
                       std::vector<std::string>());
  webview_host_id_map_.erase(host_it);
}

void WebViewContentScriptManager::RemoveContentScripts(
    int embedder_process_id,
    int view_instance_id,
    const HostID& host_id,
    const std::vector<std::string>& script_name_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  auto script_map_iter = guest_content_script_map_.find(key);
  if (script_map_iter == guest_content_script_map_.end())
    return;

  DeclarativeUserScriptMaster* master =
      DeclarativeUserScriptManager::Get(browser_context_)
          ->GetDeclarativeUserScriptMasterByID(host_id);
  CHECK(master);

  // We need to update WebViewRenderState.
  std::set<int> ids_to_delete;
  std::set<UserScriptIDPair> scripts_to_delete;

  // Step 1: removes content scripts from |master| and updates
  // |guest_content_script_map_|.
  std::map<std::string, UserScriptIDPair>& map = script_map_iter->second;
  // If the |script_name_list| is empty, all the content scripts added by the
  // guest will be removed; otherwise, removes the scripts in the
  // |script_name_list|.
  if (script_name_list.empty()) {
    auto it = map.begin();
    while (it != map.end()) {
      scripts_to_delete.insert(it->second);
      ids_to_delete.insert(it->second.id);
      map.erase(it++);
    }
  } else {
    for (const std::string& name : script_name_list) {
      auto iter = map.find(name);
      if (iter == map.end())
        continue;
      const UserScriptIDPair& id_pair = iter->second;
      ids_to_delete.insert(id_pair.id);
      scripts_to_delete.insert(id_pair);
      map.erase(iter);
    }
  }

  // Step 2: makes WebViewContentScriptManager become an observer of the
  // |loader| for scripts loaded event.
  UserScriptLoader* loader = master->loader();
  DCHECK(loader);
  if (!user_script_loader_observer_.IsObserving(loader))
    user_script_loader_observer_.Add(loader);

  // Step 3: removes content scripts from master.
  master->RemoveScripts(scripts_to_delete);

  // Step 4: updates WebViewRenderState.
  if (!ids_to_delete.empty()) {
    WebViewRendererState::GetInstance()->RemoveContentScriptIDs(
        embedder_process_id, view_instance_id, ids_to_delete);
  }
}

std::set<int> WebViewContentScriptManager::GetContentScriptIDSet(
    int embedder_process_id,
    int view_instance_id) {
  std::set<int> ids;

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  GuestContentScriptMap::const_iterator iter =
      guest_content_script_map_.find(key);
  if (iter == guest_content_script_map_.end())
    return ids;
  const ContentScriptMap& map = iter->second;
  for (const auto& id_pair : map)
    ids.insert(id_pair.second.id);

  return ids;
}

void WebViewContentScriptManager::SignalOnScriptsLoaded(
    base::OnceClosure callback) {
  if (!user_script_loader_observer_.IsObservingSources()) {
    std::move(callback).Run();
    return;
  }
  pending_scripts_loading_callbacks_.push_back(std::move(callback));
}

void WebViewContentScriptManager::OnScriptsLoaded(
    UserScriptLoader* loader,
    content::BrowserContext* browser_context) {
  user_script_loader_observer_.Remove(loader);
  RunCallbacksIfReady();
}

void WebViewContentScriptManager::OnUserScriptLoaderDestroyed(
    UserScriptLoader* loader) {
  user_script_loader_observer_.Remove(loader);
  RunCallbacksIfReady();
}

void WebViewContentScriptManager::RunCallbacksIfReady() {
  if (user_script_loader_observer_.IsObservingSources())
    return;
  for (auto& callback : pending_scripts_loading_callbacks_)
    std::move(callback).Run();
  pending_scripts_loading_callbacks_.clear();
}

}  // namespace extensions
