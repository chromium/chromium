// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_api_frame_id_map.h"

#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
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
      window_id(extension_misc::kUnknownWindowId),
      frame_type(api::extension_types::FrameType::FRAME_TYPE_OUTERMOST_FRAME),
      document_lifecycle(
          api::extension_types::DocumentLifecycle::DOCUMENT_LIFECYCLE_ACTIVE) {}

ExtensionApiFrameIdMap::FrameData::FrameData(
    int frame_id,
    int parent_frame_id,
    int tab_id,
    int window_id,
    const DocumentId& document_id,
    const DocumentId& parent_document_id,
    api::extension_types::FrameType frame_type,
    api::extension_types::DocumentLifecycle document_lifecycle)
    : frame_id(frame_id),
      parent_frame_id(parent_frame_id),
      tab_id(tab_id),
      window_id(window_id),
      document_id(document_id),
      parent_document_id(parent_document_id),
      frame_type(frame_type),
      document_lifecycle(document_lifecycle) {}

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
  if (!rfh->IsInPrimaryMainFrame())
    return rfh->GetFrameTreeNodeId();
  return kTopFrameId;
}

// static
int ExtensionApiFrameIdMap::GetFrameId(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInPrimaryMainFrame()
             ? kTopFrameId
             : navigation_handle->GetFrameTreeNodeId();
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(content::RenderFrameHost* rfh) {
  return rfh ? GetFrameId(rfh->GetParentOrOuterDocument()) : kInvalidFrameId;
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(
    content::NavigationHandle* navigation_handle) {
  return GetFrameId(navigation_handle->GetParentFrameOrOuterDocument());
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
    return web_contents->GetPrimaryMainFrame();

  DCHECK_GE(frame_id, 1);

  // Unfortunately, extension APIs do not know which process to expect for a
  // given frame ID, so we must use an unsafe API here that could return a
  // different RenderFrameHost than the caller may have expected (e.g., one that
  // changed after a cross-process navigation).
  content::RenderFrameHost* rfh =
      web_contents->UnsafeFindFrameByFrameTreeNodeId(frame_id);

  // Fail if the frame is not active or in prerendering (e.g. in the
  // back/forward cache).
  if (!rfh || (!rfh->IsActive() &&
               !rfh->IsInLifecycleState(
                   content::RenderFrameHost::LifecycleState::kPrerendering))) {
    return nullptr;
  }

  return rfh;
}

content::RenderFrameHost*
ExtensionApiFrameIdMap::GetRenderFrameHostByDocumentId(
    const DocumentId& document_id) {
  auto iter = document_id_map_.find(document_id);
  if (iter == document_id_map_.end())
    return nullptr;
  return &iter->second->render_frame_host();
}

ExtensionApiFrameIdMap::DocumentId ExtensionApiFrameIdMap::DocumentIdFromString(
    const std::string& document_id) {
  if (document_id.length() != 32)
    return DocumentId();

  base::StringPiece string_piece(document_id);
  uint64_t high = 0;
  uint64_t low = 0;
  if (!base::HexStringToUInt64(string_piece.substr(0, 16), &high) ||
      !base::HexStringToUInt64(string_piece.substr(16, 16), &low)) {
    return DocumentId();
  }

  absl::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return DocumentId();
  }
  return token.value();
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

  return FrameData(GetFrameId(rfh), GetParentFrameId(rfh), tab_id, window_id,
                   GetDocumentId(rfh),
                   GetDocumentId(rfh->GetParentOrOuterDocument()),
                   GetFrameType(rfh), GetDocumentLifecycle(rfh));
}

ExtensionApiFrameIdMap::FrameData ExtensionApiFrameIdMap::GetFrameData(
    content::GlobalRenderFrameHostId rfh_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto frame_id_iter = deleted_frame_data_map_.find(rfh_id);
  if (frame_id_iter != deleted_frame_data_map_.end())
    return frame_id_iter->second;

  return KeyToValue(rfh_id, true /* require_live_frame */);
}

ExtensionApiFrameIdMap::DocumentId ExtensionApiFrameIdMap::GetDocumentId(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This check allows callers to pass in the result from
  // GetParentOrOuterDocument() without needing to check whether the resulting
  // frame exists.
  if (!rfh)
    return DocumentId();
  return ExtensionDocumentUserData::GetOrCreateForCurrentDocument(rfh)
      ->document_id();
}

ExtensionApiFrameIdMap::DocumentId ExtensionApiFrameIdMap::GetDocumentId(
    content::NavigationHandle* navigation_handle) {
  // We can only access NavigationHandle::GetRenderFrameHost if the navigation
  // handle has committed or is waiting to commit. This is fine because
  // otherwise the documentId is useless as it will point at the old
  // document.
  if (navigation_handle->IsWaitingToCommit() ||
      navigation_handle->HasCommitted()) {
    return GetDocumentId(navigation_handle->GetRenderFrameHost());
  }
  return DocumentId();
}

api::extension_types::FrameType ExtensionApiFrameIdMap::GetFrameType(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  if (!rfh->GetParentOrOuterDocument()) {
    return api::extension_types::FRAME_TYPE_OUTERMOST_FRAME;
  }
  if (rfh->IsFencedFrameRoot()) {
    return api::extension_types::FRAME_TYPE_FENCED_FRAME;
  }
  return api::extension_types::FRAME_TYPE_SUB_FRAME;
}

api::extension_types::FrameType ExtensionApiFrameIdMap::GetFrameType(
    content::NavigationHandle* navigation_handle) {
  switch (navigation_handle->GetNavigatingFrameType()) {
    case content::FrameType::kSubframe:
      return api::extension_types::FRAME_TYPE_SUB_FRAME;
    case content::FrameType::kFencedFrameRoot:
      return api::extension_types::FRAME_TYPE_FENCED_FRAME;
    case content::FrameType::kPrimaryMainFrame:
    case content::FrameType::kPrerenderMainFrame:
      return api::extension_types::FRAME_TYPE_OUTERMOST_FRAME;
  }
}

api::extension_types::DocumentLifecycle
ExtensionApiFrameIdMap::GetDocumentLifecycle(content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  // We use IsInLifecycleState as opposed to GetLifecycleState with a switch
  // because we cannot call GetLifecycleState for speculative frames.
  if (rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kActive)) {
    return api::extension_types::DOCUMENT_LIFECYCLE_ACTIVE;
  }
  if (rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kInBackForwardCache)) {
    return api::extension_types::DOCUMENT_LIFECYCLE_CACHED;
  }
  if (rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    return api::extension_types::DOCUMENT_LIFECYCLE_PRERENDER;
  }
  if (rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPendingDeletion)) {
    return api::extension_types::DOCUMENT_LIFECYCLE_PENDING_DELETION;
  }
  return api::extension_types::DOCUMENT_LIFECYCLE_NONE;
}

api::extension_types::DocumentLifecycle
ExtensionApiFrameIdMap::GetDocumentLifecycle(
    content::NavigationHandle* navigation_handle) {
  if (content::RenderFrameHost* parent_or_outer_document =
          navigation_handle->GetParentFrameOrOuterDocument()) {
    return GetDocumentLifecycle(parent_or_outer_document);
  }
  if (navigation_handle->IsInPrerenderedMainFrame()) {
    return api::extension_types::DOCUMENT_LIFECYCLE_PRERENDER;
  } else if (navigation_handle->IsInPrimaryMainFrame()) {
    return api::extension_types::DOCUMENT_LIFECYCLE_ACTIVE;
  }
  return api::extension_types::DOCUMENT_LIFECYCLE_NONE;
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ExtensionApiFrameIdMap* self,
                        content::GlobalRenderFrameHostId key) {
                       self->deleted_frame_data_map_.erase(key);
                     },
                     base::Unretained(this), key));
}

ExtensionApiFrameIdMap::ExtensionDocumentUserData::ExtensionDocumentUserData(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<ExtensionDocumentUserData>(render_frame_host),
      document_id_(DocumentId::Create()) {
  Get()->document_id_map_[document_id_] = this;
}

ExtensionApiFrameIdMap::ExtensionDocumentUserData::
    ~ExtensionDocumentUserData() {
  Get()->document_id_map_.erase(document_id_);
}

DOCUMENT_USER_DATA_KEY_IMPL(ExtensionApiFrameIdMap::ExtensionDocumentUserData);

}  // namespace extensions
