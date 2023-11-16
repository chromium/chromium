// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/mojom/host_id.mojom.h"

using content::BrowserThread;

namespace extensions {

WebViewContentScriptManager::WebViewContentScriptManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

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
    const mojom::HostID& host_id,
    UserScriptList scripts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UserScriptLoader* loader = ExtensionSystem::Get(browser_context_)
                                 ->user_script_manager()
                                 ->GetUserScriptLoaderByID(host_id);
  DCHECK(loader);

  // We need to update WebViewRenderState.
  std::set<std::string> ids_to_add;

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  auto iter = guest_content_script_map_.find(key);

  // Step 1: Finds the entry in guest_content_script_map_ by the given |key|.
  // If there isn't any content script added for the given guest yet, insert an
  // empty map first.
  if (iter == guest_content_script_map_.end()) {
    iter = guest_content_script_map_.insert(
        iter,
        std::pair<GuestMapKey, ContentScriptMap>(key, ContentScriptMap()));
  }

  // Step 2: Updates the guest_content_script_map_.
  ContentScriptMap& map = iter->second;
  std::set<std::string> ids_to_delete;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    auto map_iter = map.find(script->name());
    // If a content script has the same name as the new one, remove the old
    // script first, and insert the new one.
    if (map_iter != map.end()) {
      ids_to_delete.insert(map_iter->second);
      map.erase(map_iter);
    }
    map.emplace(script->name(), script->id());
    ids_to_add.insert(script->id());
  }

  if (!ids_to_delete.empty()) {
    pending_operation_count_++;
    loader->RemoveScripts(
        ids_to_delete,
        base::BindOnce(&WebViewContentScriptManager::OnScriptsUpdated,
                       weak_factory_.GetWeakPtr()));
  }

  // Step 3: Adds new scripts to the set.
  pending_operation_count_++;
  loader->AddScripts(
      std::move(scripts), embedder_process_id,
      render_frame_host->GetRoutingID(),
      base::BindOnce(&WebViewContentScriptManager::OnScriptsUpdated,
                     weak_factory_.GetWeakPtr()));

  // Step 4: Creates an entry in |webview_host_id_map_| for the given
  // |embedder_process_id| and |view_instance_id| if it doesn't exist.
  auto host_it = webview_host_id_map_.find(key);
  if (host_it == webview_host_id_map_.end())
    webview_host_id_map_.insert(std::make_pair(key, host_id));

  // Step 5: Updates WebViewRenderState.
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
    const mojom::HostID& host_id,
    const std::vector<std::string>& script_name_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  auto script_map_iter = guest_content_script_map_.find(key);
  if (script_map_iter == guest_content_script_map_.end())
    return;

  UserScriptLoader* loader = ExtensionSystem::Get(browser_context_)
                                 ->user_script_manager()
                                 ->GetUserScriptLoaderByID(host_id);
  CHECK(loader);

  // We need to update WebViewRenderState.
  std::set<std::string> ids_to_delete;

  // Step 1: Removes content scripts from |set| and updates
  // |guest_content_script_map_|.
  std::map<std::string, std::string>& map = script_map_iter->second;
  // If the |script_name_list| is empty, all the content scripts added by the
  // guest will be removed; otherwise, removes the scripts in the
  // |script_name_list|.
  if (script_name_list.empty()) {
    auto it = map.begin();
    while (it != map.end()) {
      ids_to_delete.insert(it->second);
      map.erase(it++);
    }
  } else {
    for (const std::string& name : script_name_list) {
      auto iter = map.find(name);
      if (iter == map.end())
        continue;
      ids_to_delete.insert(iter->second);
      map.erase(iter);
    }
  }

  // Step 2: Removes content scripts from set.
  pending_operation_count_++;
  loader->RemoveScripts(
      ids_to_delete,
      base::BindOnce(&WebViewContentScriptManager::OnScriptsUpdated,
                     weak_factory_.GetWeakPtr()));

  // Step 3: Updates WebViewRenderState.
  if (!ids_to_delete.empty()) {
    WebViewRendererState::GetInstance()->RemoveContentScriptIDs(
        embedder_process_id, view_instance_id, ids_to_delete);
  }
}

std::set<std::string> WebViewContentScriptManager::GetContentScriptIDSet(
    int embedder_process_id,
    int view_instance_id) {
  std::set<std::string> ids;

  GuestMapKey key = std::pair<int, int>(embedder_process_id, view_instance_id);
  GuestContentScriptMap::const_iterator iter =
      guest_content_script_map_.find(key);
  if (iter == guest_content_script_map_.end())
    return ids;
  const ContentScriptMap& map = iter->second;
  for (const auto& id_pair : map)
    ids.insert(id_pair.second);

  return ids;
}

void WebViewContentScriptManager::SignalOnScriptsUpdated(
    base::OnceClosure callback) {
  if (pending_operation_count_ == 0) {
    std::move(callback).Run();
    return;
  }
  pending_scripts_loading_callbacks_.push_back(std::move(callback));
}

void WebViewContentScriptManager::OnScriptsUpdated(
    UserScriptLoader* loader,
    const std::optional<std::string>& error) {
  --pending_operation_count_;
  DCHECK_GE(pending_operation_count_, 0);
  RunCallbacksIfReady();
}

void WebViewContentScriptManager::RunCallbacksIfReady() {
  if (pending_operation_count_ > 0)
    return;
  for (auto& callback : pending_scripts_loading_callbacks_)
    std::move(callback).Run();
  pending_scripts_loading_callbacks_.clear();
}

}  // namespace extensions
