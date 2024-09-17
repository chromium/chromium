// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

WebViewRendererState::WebViewInfo::WebViewInfo() {
}

WebViewRendererState::WebViewInfo::WebViewInfo(const WebViewInfo& other) =
    default;

WebViewRendererState::WebViewInfo::~WebViewInfo() {
}

// static
WebViewRendererState* WebViewRendererState::GetInstance() {
  return base::Singleton<WebViewRendererState>::get();
}

WebViewRendererState::WebViewRendererState() {
}

WebViewRendererState::~WebViewRendererState() {
}

bool WebViewRendererState::IsGuest(int render_process_id) const {
  base::AutoLock auto_lock(web_view_partition_id_map_lock_);
  return base::Contains(web_view_partition_id_map_, render_process_id);
}

void WebViewRendererState::AddGuest(int guest_process_id,
                                    int guest_routing_id,
                                    const WebViewInfo& web_view_info) {
  base::AutoLock auto_lock(web_view_info_map_lock_);
  base::AutoLock auto_lock2(web_view_partition_id_map_lock_);

  GlobalRenderFrameHostId global_routing_id(guest_process_id, guest_routing_id);
  bool updating =
      web_view_info_map_.find(global_routing_id) != web_view_info_map_.end();
  web_view_info_map_[global_routing_id] = web_view_info;
  if (updating) {
    return;
  }

  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end()) {
    ++iter->second.web_view_frame_count;
    return;
  }
  WebViewPartitionInfo partition_info(1, web_view_info.partition_id);
  web_view_partition_id_map_[guest_process_id] = partition_info;
}

void WebViewRendererState::RemoveGuest(int guest_process_id,
                                       int guest_routing_id) {
  base::AutoLock auto_lock(web_view_info_map_lock_);
  base::AutoLock auto_lock2(web_view_partition_id_map_lock_);

  GlobalRenderFrameHostId global_routing_id(guest_process_id, guest_routing_id);
  // It's possible that we might've removed this particular frame already; in
  // this case, ensure that the refcount in web_view_partition_id_map_ isn't
  // double-decremented.  In particular, this can happen when a <webview>'s
  // process is terminated and then reloaded.
  if (web_view_info_map_.erase(global_routing_id) == 0) {
    return;
  }

  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end() &&
      iter->second.web_view_frame_count > 1) {
    --iter->second.web_view_frame_count;
    return;
  }
  web_view_partition_id_map_.erase(guest_process_id);
}

bool WebViewRendererState::GetInfo(int guest_process_id,
                                   int guest_routing_id,
                                   WebViewInfo* web_view_info) const {
  base::AutoLock auto_lock(web_view_info_map_lock_);

  GlobalRenderFrameHostId global_routing_id(guest_process_id, guest_routing_id);
  auto iter = web_view_info_map_.find(global_routing_id);
  if (iter != web_view_info_map_.end()) {
    *web_view_info = iter->second;
    return true;
  }
  return false;
}

bool WebViewRendererState::GetOwnerInfo(int guest_process_id,
                                        int* owner_process_id,
                                        std::string* owner_host) const {
  base::AutoLock auto_lock(web_view_info_map_lock_);

  // TODO(fsamuel): Store per-process info in WebViewPartitionInfo instead of in
  // WebViewInfo.
  for (const auto& info : web_view_info_map_) {
    if (info.first.child_id == guest_process_id) {
      if (owner_process_id) {
        *owner_process_id = info.second.embedder_process_id;
      }
      if (owner_host) {
        *owner_host = info.second.owner_host;
      }
      return true;
    }
  }
  return false;
}

bool WebViewRendererState::GetPartitionID(int guest_process_id,
                                          std::string* partition_id) const {
  base::AutoLock auto_lock(web_view_partition_id_map_lock_);

  auto iter = web_view_partition_id_map_.find(guest_process_id);
  if (iter != web_view_partition_id_map_.end()){
    *partition_id = iter->second.partition_id;
    return true;
  }
  return false;
}

void WebViewRendererState::AddContentScriptIDs(
    int embedder_process_id,
    int view_instance_id,
    const std::set<std::string>& script_ids) {
  base::AutoLock auto_lock(web_view_info_map_lock_);

  for (auto& render_id_info : web_view_info_map_) {
    WebViewInfo& info = render_id_info.second;
    if (info.embedder_process_id == embedder_process_id &&
        info.instance_id == view_instance_id) {
      info.content_script_ids.insert(script_ids.begin(), script_ids.end());
      // Note that it's important not to return early here, as there could be
      // multiple entries per <webview> tag with site-isolated <webview> (e.g.
      // due to OOPIFs inside <webview>).
    }
  }
}

void WebViewRendererState::RemoveContentScriptIDs(
    int embedder_process_id,
    int view_instance_id,
    const std::set<std::string>& script_ids) {
  base::AutoLock auto_lock(web_view_info_map_lock_);

  for (auto& render_id_info : web_view_info_map_) {
    WebViewInfo& info = render_id_info.second;
    if (info.embedder_process_id == embedder_process_id &&
        info.instance_id == view_instance_id) {
      for (const std::string& id : script_ids)
        info.content_script_ids.erase(id);
      // Note that it's important not to return early here, as there could be
      // multiple entries per <webview> tag with site-isolated <webview> (e.g.
      // due to OOPIFs inside <webview>).
    }
  }
}

}  // namespace extensions
