// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_api_frame_id_map.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
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
                                             int window_id)
    : frame_id(frame_id),
      parent_frame_id(parent_frame_id),
      tab_id(tab_id),
      window_id(window_id) {}

ExtensionApiFrameIdMap::FrameData::~FrameData() = default;

ExtensionApiFrameIdMap::FrameData::FrameData(
    const ExtensionApiFrameIdMap::FrameData& other) = default;
ExtensionApiFrameIdMap::FrameData& ExtensionApiFrameIdMap::FrameData::operator=(
    const ExtensionApiFrameIdMap::FrameData& other) = default;

ExtensionApiFrameIdMap::ExtensionApiFrameIdMap() = default;

ExtensionApiFrameIdMap::~ExtensionApiFrameIdMap() = default;

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
  return GetFrameId(navigation_handle->GetParentFrame());
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
    content::GlobalRenderFrameHostId key,
    bool require_live_frame) const {
  return KeyToValue(content::RenderFrameHost::FromID(key), require_live_frame);
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::KeyToValue(
    content::RenderFrameHost* rfh,
    bool require_live_frame) const {
  if (!rfh || (require_live_frame && !rfh->IsRenderFrameLive()))
    return FrameData();

  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  // The browser client can be null in unittests.
  if (ExtensionsBrowserClient::Get()) {
    ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        content::WebContents::FromRenderFrameHost(rfh), &tab_id, &window_id);
  }
  return FrameData(GetFrameId(rfh), GetParentFrameId(rfh), tab_id, window_id);
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::GetFrameData(
    content::GlobalRenderFrameHostId rfh_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto frame_id_iter = deleted_frame_data_map_.find(rfh_id);
  if (frame_id_iter != deleted_frame_data_map_.end())
    return frame_id_iter->second;

  return KeyToValue(rfh_id, true /* require_live_frame */);
}

void ExtensionApiFrameIdMap::OnRenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);

  const content::GlobalRenderFrameHostId key(rfh->GetGlobalId());
  // TODO(http://crbug.com/522129): This is necessary right now because beacon
  // requests made in window.onunload may start after this has been called.
  // Delay the RemoveFrameData() call, so we will still have the frame data
  // cached when the beacon request comes in.
  deleted_frame_data_map_.insert(
      {key, KeyToValue(rfh, false /* require_live_frame */)});
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ExtensionApiFrameIdMap* self,
                        content::GlobalRenderFrameHostId key) {
                       self->deleted_frame_data_map_.erase(key);
                     },
                     base::Unretained(this), key));
}

}  // namespace extensions
