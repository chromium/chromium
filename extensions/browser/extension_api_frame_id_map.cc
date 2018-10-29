// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_api_frame_id_map.h"

#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// The map is accessed on the IO and UI thread, so construct it once and never
// delete it.
base::LazyInstance<ExtensionApiFrameIdMap>::Leaky g_map_instance =
    LAZY_INSTANCE_INITIALIZER;

bool IsFrameRoutingIdValid(int frame_routing_id) {
  // frame_routing_id == -2 = MSG_ROUTING_NONE -> not a RenderFrameHost.
  // frame_routing_id == -1 -> should be MSG_ROUTING_NONE, but there are
  // callers that use "-1" for unknown frames.
  return frame_routing_id > -1;
}

}  // namespace

const int ExtensionApiFrameIdMap::kInvalidFrameId = -1;
const int ExtensionApiFrameIdMap::kTopFrameId = 0;

ExtensionApiFrameIdMap::FrameData::FrameData()
    : frame_id(kInvalidFrameId),
      parent_frame_id(kInvalidFrameId),
      tab_id(extension_misc::kUnknownTabId),
      window_id(extension_misc::kUnknownWindowId) {}

ExtensionApiFrameIdMap::FrameData::FrameData(int frame_id,
                                             int parent_frame_id,
                                             int tab_id,
                                             int window_id,
                                             GURL last_committed_main_frame_url)
    : frame_id(frame_id),
      parent_frame_id(parent_frame_id),
      tab_id(tab_id),
      window_id(window_id),
      last_committed_main_frame_url(std::move(last_committed_main_frame_url)) {}

ExtensionApiFrameIdMap::FrameData::~FrameData() = default;

ExtensionApiFrameIdMap::FrameData::FrameData(
    const ExtensionApiFrameIdMap::FrameData& other) = default;
ExtensionApiFrameIdMap::FrameData& ExtensionApiFrameIdMap::FrameData::operator=(
    const ExtensionApiFrameIdMap::FrameData& other) = default;

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey()
    : render_process_id(content::ChildProcessHost::kInvalidUniqueID),
      frame_routing_id(MSG_ROUTING_NONE) {}

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey(
    int render_process_id,
    int frame_routing_id)
    : render_process_id(render_process_id),
      frame_routing_id(frame_routing_id) {}

ExtensionApiFrameIdMap::FrameDataCallbacks::FrameDataCallbacks()
    : is_iterating(false) {}

ExtensionApiFrameIdMap::FrameDataCallbacks::FrameDataCallbacks(
    const FrameDataCallbacks& other) = default;

ExtensionApiFrameIdMap::FrameDataCallbacks::~FrameDataCallbacks() {}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator<(
    const RenderFrameIdKey& other) const {
  return std::tie(render_process_id, frame_routing_id) <
         std::tie(other.render_process_id, other.frame_routing_id);
}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator==(
    const RenderFrameIdKey& other) const {
  return render_process_id == other.render_process_id &&
         frame_routing_id == other.frame_routing_id;
}

ExtensionApiFrameIdMap::ExtensionApiFrameIdMap() {
  // The browser client can be null in unittests.
  if (ExtensionsBrowserClient::Get()) {
    helper_ =
        ExtensionsBrowserClient::Get()->CreateExtensionApiFrameIdMapHelper(
            this);
  }
}

ExtensionApiFrameIdMap::~ExtensionApiFrameIdMap() {}

// static
ExtensionApiFrameIdMap* ExtensionApiFrameIdMap::Get() {
  return g_map_instance.Pointer();
}

// static
int ExtensionApiFrameIdMap::GetFrameId(content::RenderFrameHost* rfh) {
  if (!rfh)
    return kInvalidFrameId;
  if (rfh->GetParent())
    return rfh->GetFrameTreeNodeId();
  return kTopFrameId;
}

// static
int ExtensionApiFrameIdMap::GetFrameId(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame()
             ? kTopFrameId
             : navigation_handle->GetFrameTreeNodeId();
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(content::RenderFrameHost* rfh) {
  return rfh ? GetFrameId(rfh->GetParent()) : kInvalidFrameId;
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return kInvalidFrameId;

  if (navigation_handle->IsParentMainFrame())
    return kTopFrameId;

  return navigation_handle->GetParentFrame()->GetFrameTreeNodeId();
}

// static
content::RenderFrameHost* ExtensionApiFrameIdMap::GetRenderFrameHostById(
    content::WebContents* web_contents,
    int frame_id) {
  // Although it is technically possible to map |frame_id| to a RenderFrameHost
  // without WebContents, we choose to not do that because in the extension API
  // frameIds are only guaranteed to be meaningful in combination with a tabId.
  if (!web_contents)
    return nullptr;

  if (frame_id == kInvalidFrameId)
    return nullptr;

  if (frame_id == kTopFrameId)
    return web_contents->GetMainFrame();

  DCHECK_GE(frame_id, 1);

  // Unfortunately, extension APIs do not know which process to expect for a
  // given frame ID, so we must use an unsafe API here that could return a
  // different RenderFrameHost than the caller may have expected (e.g., one that
  // changed after a cross-process navigation).
  return web_contents->UnsafeFindFrameByFrameTreeNodeId(frame_id);
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::KeyToValue(
    const RenderFrameIdKey& key) const {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      key.render_process_id, key.frame_routing_id);

  if (!rfh || !rfh->IsRenderFrameLive())
    return FrameData();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  // The RenderFrameHost may not have an associated WebContents in cases
  // such as interstitial pages.
  GURL last_committed_main_frame_url =
      web_contents ? web_contents->GetLastCommittedURL() : GURL();
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  if (helper_)
    helper_->PopulateTabData(rfh, &tab_id, &window_id);
  return FrameData(GetFrameId(rfh), GetParentFrameId(rfh), tab_id, window_id,
                   std::move(last_committed_main_frame_url));
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::LookupFrameDataOnUI(
    const RenderFrameIdKey& key,
    bool is_from_io) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool lookup_successful = false;
  FrameData data;
  FrameDataMap::const_iterator frame_id_iter = frame_data_map_.find(key);
  if (frame_id_iter != frame_data_map_.end()) {
    lookup_successful = true;
    data = frame_id_iter->second;
  } else {
    data = KeyToValue(key);
    // Don't save invalid values in the map.
    if (data.frame_id != kInvalidFrameId) {
      lookup_successful = true;
      auto kvpair = FrameDataMap::value_type(key, data);
      base::AutoLock lock(frame_data_map_lock_);
      frame_data_map_.insert(kvpair);
    }
  }

  // TODO(devlin): Depending on how the data looks, this may be removable after
  // a few cycles. Check back in M52 to see if it's still needed.
  if (is_from_io) {
    UMA_HISTOGRAM_BOOLEAN("Extensions.ExtensionFrameMapLookupSuccessful",
                          lookup_successful);
  }

  return data;
}

void ExtensionApiFrameIdMap::ReceivedFrameDataOnIO(
    const RenderFrameIdKey& key,
    const FrameData& cached_frame_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto map_iter = callbacks_map_.find(key);
  if (map_iter == callbacks_map_.end()) {
    // Can happen if ReceivedFrameDataOnIO was called after the frame ID was
    // resolved (e.g. via GetFrameDataOnIO), but before PostTaskAndReply
    // replied.
    return;
  }

  FrameDataCallbacks& callbacks = map_iter->second;

  if (callbacks.is_iterating)
    return;
  callbacks.is_iterating = true;

  // Note: Extra items can be appended to |callbacks| during this loop if a
  // callback calls GetFrameDataOnIO().
  for (auto it = callbacks.callbacks.begin(); it != callbacks.callbacks.end();
       ++it) {
    it->Run(cached_frame_data);
  }
  callbacks_map_.erase(key);
}

void ExtensionApiFrameIdMap::GetFrameDataOnIO(
    int render_process_id,
    int frame_routing_id,
    const FrameDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(robwu): Enable assertion when all callers have been fixed.
  // DCHECK_EQ(MSG_ROUTING_NONE, -1);
  if (!IsFrameRoutingIdValid(frame_routing_id)) {
    callback.Run(FrameData());
    return;
  }

  FrameData cached_frame_data;
  bool did_find_cached_frame_data = GetCachedFrameDataOnIO(
      render_process_id, frame_routing_id, &cached_frame_data);

  const RenderFrameIdKey key(render_process_id, frame_routing_id);
  auto map_iter = callbacks_map_.find(key);

  if (did_find_cached_frame_data) {
    // Value already cached, thread hopping is not needed.
    if (map_iter == callbacks_map_.end()) {
      // If the frame ID was cached, then it is likely that there are no pending
      // callbacks. So do not unnecessarily copy the callback, but run it.
      callback.Run(cached_frame_data);
    } else {
      map_iter->second.callbacks.push_back(callback);
      ReceivedFrameDataOnIO(key, cached_frame_data);
    }
    return;
  }

  // The key was seen for the first time (or the frame has been removed).
  // Hop to the UI thread to look up the extension API frame ID.
  callbacks_map_[key].callbacks.push_back(callback);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&ExtensionApiFrameIdMap::LookupFrameDataOnUI,
                 base::Unretained(this), key, true /* is_from_io */),
      base::Bind(&ExtensionApiFrameIdMap::ReceivedFrameDataOnIO,
                 base::Unretained(this), key));
}

bool ExtensionApiFrameIdMap::GetCachedFrameDataOnIO(int render_process_id,
                                                    int frame_routing_id,
                                                    FrameData* frame_data_out) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(robwu): Enable assertion when all callers have been fixed.
  // DCHECK_EQ(MSG_ROUTING_NONE, -1);
  if (!IsFrameRoutingIdValid(frame_routing_id))
    return false;

  // A valid routing ID is only meaningful with a valid process ID.
  DCHECK_GE(render_process_id, 0);

  bool found = false;
  {
    base::AutoLock lock(frame_data_map_lock_);
    FrameDataMap::const_iterator frame_id_iter = frame_data_map_.find(
        RenderFrameIdKey(render_process_id, frame_routing_id));
    if (frame_id_iter != frame_data_map_.end()) {
      // This is very likely to happen because CacheFrameData() is called as
      // soon as the frame is created.
      *frame_data_out = frame_id_iter->second;
      found = true;
    }
  }

  // TODO(devlin): Depending on how the data looks, this may be removable after
  // a few cycles. Check back in M52 to see if it's still needed.
  UMA_HISTOGRAM_BOOLEAN("Extensions.ExtensionFrameMapCacheHit", found);
  return found;
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::GetFrameData(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!rfh)
    return FrameData();

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  return LookupFrameDataOnUI(key, false /* is_from_io */);
}

void ExtensionApiFrameIdMap::InitializeRenderFrameData(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  DCHECK(rfh->IsRenderFrameLive());

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  CacheFrameData(key);
  DCHECK(frame_data_map_.find(key) != frame_data_map_.end());
}

void ExtensionApiFrameIdMap::CacheFrameData(const RenderFrameIdKey& key) {
  LookupFrameDataOnUI(key, false /* is_from_io */);
}

void ExtensionApiFrameIdMap::OnRenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  RemoveFrameData(key);
}

void ExtensionApiFrameIdMap::UpdateTabAndWindowId(
    int tab_id,
    int window_id,
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());

  // Only track FrameData for live render frames.
  if (!rfh->IsRenderFrameLive()) {
    return;
  }

  base::AutoLock lock(frame_data_map_lock_);
  auto iter = frame_data_map_.find(key);
  // The FrameData for |rfh| should have already been initialized.
  DCHECK(iter != frame_data_map_.end());
  iter->second.tab_id = tab_id;
  iter->second.window_id = window_id;
}

void ExtensionApiFrameIdMap::OnMainFrameReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  bool did_insert = false;
  std::tie(std::ignore, did_insert) =
      ready_to_commit_document_navigations_.insert(navigation_handle);
  DCHECK(did_insert);

  content::RenderFrameHost* main_frame =
      navigation_handle->GetRenderFrameHost();
  DCHECK(main_frame);

  // We only track live frames.
  if (!main_frame->IsRenderFrameLive())
    return;

  const RenderFrameIdKey key(main_frame->GetProcess()->GetID(),
                             main_frame->GetRoutingID());
  base::AutoLock lock(frame_data_map_lock_);
  auto iter = frame_data_map_.find(key);

  // We must have already cached the FrameData for this in
  // InitializeRenderFrameHost.
  DCHECK(iter != frame_data_map_.end());
  iter->second.pending_main_frame_url = navigation_handle->GetURL();
}

void ExtensionApiFrameIdMap::OnMainFrameDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  bool did_fire_ready_to_commit_navigation =
      !!ready_to_commit_document_navigations_.erase(navigation_handle);

  // It's safe to call NavigationHandle::GetRenderFrameHost here iff the
  // navigation committed or a ReadyToCommitNavigation event was dispatched for
  // this navigation.
  // Note a RenderFrameHost might not be associated with the NavigationHandle in
  // WebContentsObserver::DidFinishNavigation. This might happen when the
  // navigation doesn't commit which might happen for a variety of reasons like
  // the network network request to fetch the navigation url failed, the
  // navigation was cancelled, by say a NavigationThrottle etc.
  // There's nothing to do if the RenderFrameHost can't be fetched for this
  // navigation.
  bool can_fetch_render_frame_host =
      navigation_handle->HasCommitted() || did_fire_ready_to_commit_navigation;
  if (!can_fetch_render_frame_host)
    return;

  content::RenderFrameHost* main_frame =
      navigation_handle->GetRenderFrameHost();
  DCHECK(main_frame);

  // We only track live frames.
  if (!main_frame->IsRenderFrameLive())
    return;

  const RenderFrameIdKey key(main_frame->GetProcess()->GetID(),
                             main_frame->GetRoutingID());
  base::AutoLock lock(frame_data_map_lock_);
  auto iter = frame_data_map_.find(key);

  // We must have already cached the FrameData for this in
  // InitializeRenderFrameHost.
  DCHECK(iter != frame_data_map_.end());
  iter->second.last_committed_main_frame_url =
      main_frame->GetLastCommittedURL();
  iter->second.pending_main_frame_url = base::nullopt;
}

bool ExtensionApiFrameIdMap::HasCachedFrameDataForTesting(
    content::RenderFrameHost* rfh) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!rfh)
    return false;

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  return frame_data_map_.find(key) != frame_data_map_.end();
}

size_t ExtensionApiFrameIdMap::GetFrameDataCountForTesting() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return frame_data_map_.size();
}

void ExtensionApiFrameIdMap::RemoveFrameData(const RenderFrameIdKey& key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::AutoLock lock(frame_data_map_lock_);
  frame_data_map_.erase(key);
}

}  // namespace extensions
