// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/supports_user_data.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

namespace extensions::mime_handler {

namespace {

// Static factory instance (always nullptr for non-test).
MimeHandlerStreamManager::Factory* g_factory = nullptr;

// Creates a claimed `EmbedderHostInfo` from the `embedder_host`.
MimeHandlerStreamManager::EmbedderHostInfo GetEmbedderHostInfo(
    const content::RenderFrameHost* embedder_host) {
  return {embedder_host->GetFrameTreeNodeId(), embedder_host->GetGlobalId()};
}

// Creates a new unclaimed `EmbedderHostInfo` for the given frame tree node ID
// (without the `content::GlobalRenderFrameHostId`).
MimeHandlerStreamManager::EmbedderHostInfo GetUnclaimedEmbedderHostInfo(
    content::FrameTreeNodeId frame_tree_node_id) {
  return {frame_tree_node_id, content::GlobalRenderFrameHostId()};
}

// Some clients of this class require knowing whether a navigation targets a
// content frame served by a MIME handler. Today PDF is the only such client,
// so the predicate reduces to `IsPdf()`.
// TODO(crbug.com/495538206): find a better way to do that.
bool IsContentFrameNavigation(content::NavigationHandle* navigation) {
  return navigation->IsPdf();
}

// Gets the embedder host from the content host's navigation handle.
content::RenderFrameHost* GetEmbedderHostFromContentNavigation(
    content::NavigationHandle* navigation_handle) {
  // Since `navigation_handle` is for a content frame, the parent frame is
  // the extension frame, and the grandparent frame is the embedder frame.
  content::RenderFrameHost* extension_host =
      navigation_handle->GetParentFrame();
  CHECK(extension_host);

  return extension_host->GetParent();
}

// Gets the `extensions::mojom::MimeHandlerViewContainerManager` from the
// `container_host`.
mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
GetMimeHandlerViewContainerManager(content::RenderFrameHost* container_host) {
  CHECK(container_host);

  mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
      container_manager;
  container_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &container_manager);
  return container_manager;
}

// Debugging data for crbug.com/391459596.
// TODO(crbug.com/391459596): Remove once fixed.
struct MimeHandlerNavigationDebugData : public base::SupportsUserData::Data {
  bool did_start_navigation = false;
  bool did_start_navigation_with_parent = false;
};

static int g_debug_manager_instances = 0;
static int g_debug_ongoing_content_navigations = 0;

// Manager crash keys.
static crash_reporter::CrashKeyString<32> crash_key_manager_instances(
    "mime-handler-manager-instances");
static crash_reporter::CrashKeyString<32> crash_key_stream_count(
    "mime-handler-stream-count");
static crash_reporter::CrashKeyString<32> crash_key_ongoing_content_navigations(
    "mime-handler-ongoing-content-navigations");

// Content navigation crash keys.
static crash_reporter::CrashKeyString<6> crash_key_did_start_navigation(
    "mime-handler-did-start-navigation");
static crash_reporter::CrashKeyString<6>
    crash_key_did_start_navigation_with_parent(
        "mime-handler-did-start-navigation-with-parent");

void SetManagerCrashKeys(size_t stream_count) {
  crash_key_manager_instances.Set(base::ToString(g_debug_manager_instances));
  crash_key_stream_count.Set(base::ToString(stream_count));
  crash_key_ongoing_content_navigations.Set(
      base::ToString(g_debug_ongoing_content_navigations));
}

void SetContentNavigationCrashKeys(
    const content::NavigationHandle* navigation_handle,
    const void* key) {
  auto* data = navigation_handle->GetUserData(key);
  if (!data) {
    // Can be nullptr in tests.
    return;
  }

  auto* debug_data = static_cast<MimeHandlerNavigationDebugData*>(data);

  crash_key_did_start_navigation.Set(
      base::ToString(debug_data->did_start_navigation));
  crash_key_did_start_navigation_with_parent.Set(
      base::ToString(debug_data->did_start_navigation_with_parent));
}

void ClearContentNavigationCrashKeys() {
  crash_key_did_start_navigation.Clear();
  crash_key_did_start_navigation_with_parent.Clear();
}

}  // namespace

bool MimeHandlerStreamManager::EmbedderHostInfo::operator<(
    const MimeHandlerStreamManager::EmbedderHostInfo& other) const {
  return std::tie(frame_tree_node_id, global_id) <
         std::tie(other.frame_tree_node_id, other.global_id);
}

MimeHandlerStreamManager::MimeHandlerStreamManager(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<MimeHandlerStreamManager>(*contents) {
  ++g_debug_manager_instances;
}

MimeHandlerStreamManager::~MimeHandlerStreamManager() {
  --g_debug_manager_instances;
}

// static
void MimeHandlerStreamManager::Create(content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }

  if (g_factory) {
    g_factory->CreateMimeHandlerStreamManager(contents);
  } else {
    // Using `new` to access a non-public constructor.
    contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new MimeHandlerStreamManager(contents)));
  }
}

// static
MimeHandlerStreamManager* MimeHandlerStreamManager::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host));
}

// static
void MimeHandlerStreamManager::SetFactoryForTesting(Factory* factory) {
  if (factory) {
    CHECK(!g_factory);
  }
  g_factory = factory;
}

void MimeHandlerStreamManager::AddStreamContainer(
    content::FrameTreeNodeId frame_tree_node_id,
    const std::string& internal_id,
    std::unique_ptr<extensions::StreamContainer> stream_container,
    std::unique_ptr<extensions::MimeHandlerStreamDelegate> delegate) {
  CHECK(stream_container);
  CHECK(delegate);

  // If an entry with the same frame tree node ID already exists in
  // `stream_infos_`, then a new navigation has occurred. If the existing
  // `StreamInfo` hasn't been claimed, replace the entry. This is safe, since
  // `GetStreamContainer()` verifies the original URL. If the existing
  // `StreamInfo` has been claimed, then it will eventually be deleted, and the
  // new `StreamInfo` will be used instead. This can occur if a full page MIME
  // handler refreshes or navigates to another handled URL.
  auto embedder_host_info = GetUnclaimedEmbedderHostInfo(frame_tree_node_id);
  stream_infos_[embedder_host_info] = std::make_unique<extensions::StreamInfo>(
      internal_id, std::move(stream_container), std::move(delegate));
}

base::WeakPtr<extensions::StreamContainer>
MimeHandlerStreamManager::GetStreamContainer(
    content::RenderFrameHost* embedder_host) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return nullptr;
  }

  // It's possible to have multiple `extensions::StreamContainer`s under the
  // same frame tree node ID. Verify the original URL in the stream container to
  // avoid a potential URL spoof.
  if (embedder_host->GetLastCommittedURL() !=
      stream_info->stream()->original_url()) {
    return nullptr;
  }

  return stream_info->stream()->GetWeakPtr();
}

bool MimeHandlerStreamManager::IsExtensionHost(
    const content::RenderFrameHost* render_frame_host) const {
  // The extension host should always have a parent host (the embedder host).
  const content::RenderFrameHost* parent_host = render_frame_host->GetParent();
  if (!parent_host) {
    return false;
  }

  return IsExtensionFrameTreeNodeId(parent_host,
                                    render_frame_host->GetFrameTreeNodeId());
}

std::optional<ExtensionId>
MimeHandlerStreamManager::GetTopLevelHandlerExtensionId() const {
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  CHECK(main_rfh);
  const extensions::StreamInfo* info = GetClaimedStreamInfo(main_rfh);
  if (!info || !info->stream()) {
    return std::nullopt;
  }
  if (info->stream()->embedded()) {
    return std::nullopt;
  }
  // It's possible to have multiple `extensions::StreamContainer`s under the
  // same frame tree node ID. Verify the original URL in the stream container
  // to avoid a potential URL spoof -- the same guard `GetStreamContainer()`
  // applies.
  if (main_rfh->GetLastCommittedURL() != info->stream()->original_url()) {
    return std::nullopt;
  }
  return info->stream()->extension_id();
}

bool MimeHandlerStreamManager::IsExtensionFrameTreeNodeId(
    const content::RenderFrameHost* embedder_host,
    content::FrameTreeNodeId frame_tree_node_id) const {
  const auto* stream_info = GetClaimedStreamInfo(embedder_host);
  return stream_info &&
         frame_tree_node_id ==
             stream_info->extension_host_frame_tree_node_id() &&
         embedder_host->GetLastCommittedURL().EqualsIgnoringRef(
             stream_info->stream()->original_url());
}

bool MimeHandlerStreamManager::DidExtensionFrameFinishNavigation(
    const content::RenderFrameHost* embedder_host) const {
  const auto* stream_info = GetClaimedStreamInfo(embedder_host);
  return stream_info && stream_info->did_extension_finish_navigation();
}

bool MimeHandlerStreamManager::IsContentHost(
    const content::RenderFrameHost* render_frame_host) const {
  // The content host should always have a parent host.
  content::RenderFrameHost* parent_host = render_frame_host->GetParent();
  if (!parent_host) {
    return false;
  }

  // The parent host should always be the extension host.
  if (!IsExtensionHost(parent_host)) {
    return false;
  }

  // The extension host should always have a parent host (the embedder host).
  content::RenderFrameHost* embedder_host = parent_host->GetParent();
  CHECK(embedder_host);
  return IsContentFrameTreeNodeId(embedder_host,
                                  render_frame_host->GetFrameTreeNodeId());
}

bool MimeHandlerStreamManager::IsContentFrameTreeNodeId(
    const content::RenderFrameHost* embedder_host,
    content::FrameTreeNodeId frame_tree_node_id) const {
  const auto* stream_info = GetClaimedStreamInfo(embedder_host);
  return stream_info &&
         frame_tree_node_id == stream_info->content_host_frame_tree_node_id() &&
         embedder_host->GetLastCommittedURL().EqualsIgnoringRef(
             stream_info->stream()->original_url());
}

bool MimeHandlerStreamManager::DidContentFrameFinishNavigation(
    const content::RenderFrameHost* embedder_host) const {
  const auto* stream_info = GetClaimedStreamInfo(embedder_host);
  return stream_info && stream_info->DidContentFrameFinishNavigation();
}

void MimeHandlerStreamManager::AbortAndFallbackToNativeHandler(
    content::RenderFrameHost* embedder_host) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  CHECK(stream_info);
  CHECK(stream_info->did_extension_finish_navigation());
  CHECK(!stream_info->DidContentFrameFinishNavigation());
  const GURL original_url = stream_info->stream()->original_url();
  CHECK(original_url.is_valid());

  const content::FrameTreeNodeId embedder_ftn =
      embedder_host->GetFrameTreeNodeId();

  // Capture the buffered body (if any) before tearing the stream down.
  // An invalid handle -- no cache attached or the source still draining
  // -- falls through to a network refetch on reload. Capture the
  // decoded byte count alongside the pipe so the throttle can populate
  // `URLLoaderCompletionStatus::decoded_body_length` correctly when it
  // replays the body (the cache stores post-decoding bytes, so the
  // wire `Content-Length` is wrong here whenever the original was
  // content-encoded).
  mojo::ScopedDataPipeConsumerHandle body =
      stream_info->stream()->GetFallbackDataPipe();
  const size_t decoded_body_size =
      body.is_valid() ? stream_info->stream()->GetCachedBodySize() : 0u;
  pending_native_fallback_frames_[embedder_ftn] =
      CachedFallbackBody{std::move(body), decoded_body_size};

  // Re-navigate just the embedder frame -- not the whole WebContents --
  // so iframe-hosted MIME handlers fall back without blowing away the
  // main frame. For a primary-main-frame embedder this is equivalent to
  // a main-frame reload. FTN is stable across the scoped navigation, so
  // the throttle's FTN-keyed peek matches the mark set here.
  content::NavigationController::LoadURLParams params(original_url);
  params.frame_tree_node_id = embedder_ftn;
  params.transition_type = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  // The embedder is already committed on this URL. Without a reload
  // classification, re-navigating to the same URL is treated as a
  // same-document scroll when it carries a fragment, so the response
  // throttle never re-runs to hand the body to the native handler.
  params.reload_type = content::ReloadType::NORMAL;
  web_contents()->GetController().LoadURLWithParams(params);
}

bool MimeHandlerStreamManager::IsPendingNativeFallback(
    content::FrameTreeNodeId frame_tree_node_id) const {
  return pending_native_fallback_frames_.contains(frame_tree_node_id);
}

std::optional<MimeHandlerStreamManager::CachedFallbackBody>
MimeHandlerStreamManager::TakeCachedFallbackBody(
    content::FrameTreeNodeId frame_tree_node_id) {
  auto it = pending_native_fallback_frames_.find(frame_tree_node_id);
  if (it == pending_native_fallback_frames_.end() ||
      !it->second.pipe.is_valid()) {
    return std::nullopt;
  }
  return std::move(it->second);
}

bool MimeHandlerStreamManager::PluginCanSave(
    const content::RenderFrameHost* embedder_host) const {
  const auto* stream_info = GetClaimedStreamInfo(embedder_host);
  return stream_info && stream_info->delegate()->PluginCanSave();
}

void MimeHandlerStreamManager::SetPluginCanSave(
    content::RenderFrameHost* embedder_host,
    bool plugin_can_save) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return;
  }

  stream_info->delegate()->SetPluginCanSave(plugin_can_save);
}

bool MimeHandlerStreamManager::ContainsUnclaimedStreamInfo(
    content::FrameTreeNodeId frame_tree_node_id) const {
  return stream_infos_.contains(
      GetUnclaimedEmbedderHostInfo(frame_tree_node_id));
}

void MimeHandlerStreamManager::DeleteUnclaimedStreamInfo(
    content::FrameTreeNodeId frame_tree_node_id) {
  CHECK(stream_infos_.erase(GetUnclaimedEmbedderHostInfo(frame_tree_node_id)));

  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

void MimeHandlerStreamManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // When the embedder frame is deleted, delete its stream.
  if (GetClaimedStreamInfo(render_frame_host)) {
    DeleteClaimedStreamInfo(render_frame_host);
    // DO NOT add code past this point. `this` may have been deleted.
    return;
  }

  // If `render_frame_host` isn't active, ignore. An unclaimed `StreamInfo`'s
  // FrameTreeNode may delete a speculative `content::RenderFrameHost` before
  // the embedder `content::RenderFrameHost` commits and claims the stream. The
  // speculative `content::RenderFrameHost` won't be considered active, and
  // shouldn't cause the stream to be deleted.
  if (!render_frame_host->IsActive()) {
    return;
  }

  // If `render_frame_host` is an unrelated host (there isn't an unclaimed
  // stream), ignore.
  content::FrameTreeNodeId frame_tree_node_id =
      render_frame_host->GetFrameTreeNodeId();
  if (!ContainsUnclaimedStreamInfo(frame_tree_node_id)) {
    return;
  }

  DeleteUnclaimedStreamInfo(frame_tree_node_id);
  // DO NOT add code past this point. `this` may have been deleted.
}

void MimeHandlerStreamManager::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // If the `old_host` is null, then it means that a subframe is being created.
  // Don't treat this like a host change.
  if (!old_host) {
    return;
  }

  if (MaybeDeleteStreamOnExtensionHostChanged(old_host) ||
      MaybeDeleteStreamOnContentHostChanged(old_host)) {
    // DO NOT add code past this point. `this` may have been deleted.
    return;
  }

  // If this is an unrelated host, ignore.
  if (!GetClaimedStreamInfo(old_host)) {
    return;
  }

  // The `old_host`'s `StreamInfo` should be deleted since this event could be
  // triggered from navigating the embedder host to a non-MIME-handler URL. If
  // the embedder host is navigating to another handled URL, then a new
  // `StreamInfo` should have already been created and claimed by `new_host`, so
  // it's still safe to delete `old_host`'s `StreamInfo`.
  DeleteClaimedStreamInfo(old_host);
  // DO NOT add code past this point. `this` may have been deleted.
}

void MimeHandlerStreamManager::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  // Drop any pending native-fallback mark keyed by the deleted frame.
  pending_native_fallback_frames_.erase(frame_tree_node_id);

  // If a MIME handler host is deleted, delete the associated `StreamInfo`.
  for (auto iter = stream_infos_.begin(); iter != stream_infos_.end();) {
    extensions::StreamInfo* stream_info = iter->second.get();
    // Check if `frame_tree_node_id` matches the embedder, extension, or
    // content host's frame tree node ID. The content host is optional and
    // may not be present for all MIME handler types.
    if (frame_tree_node_id == iter->first.frame_tree_node_id ||
        frame_tree_node_id ==
            stream_info->extension_host_frame_tree_node_id() ||
        frame_tree_node_id == stream_info->content_host_frame_tree_node_id()) {
      if (stream_info->mime_handler_view_container_manager()) {
        stream_info->mime_handler_view_container_manager()
            ->DestroyFrameContainer(stream_info->instance_id());
      }

      iter = stream_infos_.erase(iter);
    } else {
      ++iter;
    }
  }

  // Delete `this` if there are no remaining stream infos.
  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

void MimeHandlerStreamManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Set the content host frame tree node ID if the navigation is for a content
  // host. This needs to occur before the network request for the content
  // navigation so that delegates that consult content-host state during
  // navigation throttling (e.g.
  // `ChromePdfStreamDelegate::ShouldAllowPdfFrameNavigation`) see it set.
  if (IsContentFrameNavigation(navigation_handle)) {
    ++g_debug_ongoing_content_navigations;
    auto debug_data = std::make_unique<MimeHandlerNavigationDebugData>();
    debug_data->did_start_navigation = true;
    debug_data->did_start_navigation_with_parent =
        navigation_handle->GetParentFrame();
    navigation_handle->SetUserData(UserDataKey(), std::move(debug_data));
    SetManagerCrashKeys(stream_infos_.size());
    SetContentNavigationCrashKeys(navigation_handle, UserDataKey());

    SetStreamContentHostFrameTreeNodeId(navigation_handle);
  }
}

void MimeHandlerStreamManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (IsContentFrameNavigation(navigation_handle)) {
    SetManagerCrashKeys(stream_infos_.size());
    SetContentNavigationCrashKeys(navigation_handle, UserDataKey());
  }

  // Maybe register a PDF subresource override in the PDF content host.
  if (MaybeRegisterPdfSubresourceOverride(navigation_handle)) {
    return;
  }

  // Extension frame navigation: when the extension frame (child of the
  // embedder) navigates to the handler URL, dispatch to the delegate.
  // The default implementation is a no-op; subclasses can override to
  // hook into the extension frame's ReadyToCommit.
  if (auto* parent_host = navigation_handle->GetParentFrame()) {
    if (auto* stream_info = GetClaimedStreamInfo(parent_host);
        stream_info && stream_info->DidExtensionStartNavigation() &&
        stream_info->extension_host_frame_tree_node_id() ==
            navigation_handle->GetFrameTreeNodeId()) {
      stream_info->delegate()->OnExtensionFrameReadyToCommit(navigation_handle,
                                                             stream_info);
      return;
    }
  }

  // The initial load notification for the URL being served in the embedder
  // host. The `embedder_host` should claim the unclaimed `StreamInfo`. This
  // should replace any existing `StreamInfo` objects related to
  // `embedder_host`. This is safe since `GetStreamContainer()` checks the
  // original URL for URL spoofs, and any security-relevant changes in the
  // response should result in a different `content::RenderFrameHost`.
  content::RenderFrameHost* embedder_host =
      navigation_handle->GetRenderFrameHost();
  if (!ContainsUnclaimedStreamInfo(embedder_host->GetFrameTreeNodeId())) {
    return;
  }

  extensions::StreamInfo* claimed_stream_info = ClaimStreamInfo(embedder_host);
  claimed_stream_info->delegate()->OnStreamClaimed(embedder_host,
                                                   claimed_stream_info);
}

void MimeHandlerStreamManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Drop any native-fallback mark for the navigating frame. The mark
  // must survive the full redirect chain (so the throttle's peek hits
  // on each hop and the PDF extension_id is selected regardless of
  // redirects), but by the time the navigation has committed or
  // errored the re-fetch is over and the mark is spent. For a canceled
  // navigation this still fires, so the entry is never leaked.
  pending_native_fallback_frames_.erase(
      navigation_handle->GetFrameTreeNodeId());

  if (IsContentFrameNavigation(navigation_handle)) {
    --g_debug_ongoing_content_navigations;
    SetManagerCrashKeys(stream_infos_.size());
    ClearContentNavigationCrashKeys();
  }

  // Maybe set up postMessage support after the PDF content host finishes
  // navigating.
  if (MaybeSetUpPostMessage(navigation_handle)) {
    return;
  }

  // The rest of the method handles the extension host. The parent host should
  // be the tracked embedder host.
  content::RenderFrameHost* embedder_host = navigation_handle->GetParentFrame();
  if (!embedder_host) {
    return;
  }

  // The `StreamInfo` should already have been claimed by the time the extension
  // host navigates.
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  if (stream_info->DidExtensionStartNavigation()) {
    // If the extension host has already started its navigation to the handler
    // extension URL, set the extension as finished navigating. Ignore
    // navigations in other children of the embedder host. Ignore all other
    // URLs, which was shown to be possible in crbug.com/432497344.
    const GURL pdf_extension_url = stream_info->stream()->handler_url();
    if (url == pdf_extension_url &&
        stream_info->extension_host_frame_tree_node_id() ==
            navigation_handle->GetFrameTreeNodeId() &&
        navigation_handle->HasCommitted() &&
        !navigation_handle->IsErrorPage()) {
      stream_info->SetDidExtensionFinishNavigation();
      stream_info->delegate()->OnExtensionFrameFinished(navigation_handle,
                                                        stream_info);
    }
    return;
  }

  // During MIME handler navigation, in the embedder host, an about:blank embed
  // is inserted in a synthetic HTML document as a placeholder for the handler
  // extension. Navigate the about:blank embed to the handler extension URL to
  // load the extension.
  if (!url.IsAboutBlank()) {
    return;
  }

  content::RenderFrameHost* about_blank_host =
      navigation_handle->GetRenderFrameHost();
  if (!about_blank_host) {
    return;
  }

  // `about_blank_host`'s FrameTreeNode will be reused for the extension
  // `content::RenderFrameHost`, so it is safe to set it in `stream_info` to
  // identify both hosts.
  content::FrameTreeNodeId extension_host_frame_tree_node_id =
      about_blank_host->GetFrameTreeNodeId();
  stream_info->set_extension_host_frame_tree_node_id(
      extension_host_frame_tree_node_id);

  NavigateToExtensionUrl(extension_host_frame_tree_node_id, stream_info,
                         embedder_host->GetSiteInstance(),
                         about_blank_host->GetGlobalId());
}

void MimeHandlerStreamManager::ClaimStreamInfoForTesting(  // IN-TEST
    content::RenderFrameHost* embedder_host) {
  ClaimStreamInfo(embedder_host);
}

extensions::StreamInfo*
MimeHandlerStreamManager::GetClaimedStreamInfoForTesting(  // IN-TEST
    content::RenderFrameHost* embedder_host) {
  return GetClaimedStreamInfo(embedder_host);
}

void MimeHandlerStreamManager::
    SetExtensionFrameTreeNodeIdForTesting(  // IN-TEST
        content::RenderFrameHost* embedder_host,
        content::FrameTreeNodeId frame_tree_node_id) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  CHECK(stream_info);

  stream_info->set_extension_host_frame_tree_node_id(frame_tree_node_id);
}

void MimeHandlerStreamManager::SetContentFrameTreeNodeIdForTesting(  // IN-TEST
    content::RenderFrameHost* embedder_host,
    content::FrameTreeNodeId frame_tree_node_id) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  CHECK(stream_info);

  stream_info->set_content_host_frame_tree_node_id(frame_tree_node_id);
}

void MimeHandlerStreamManager::NavigateToExtensionUrl(
    content::FrameTreeNodeId extension_host_frame_tree_node_id,
    extensions::StreamInfo* stream_info,
    content::SiteInstance* source_site_instance,
    content::GlobalRenderFrameHostId global_id) {
  CHECK(stream_info);

  content::NavigationController::LoadURLParams params(
      stream_info->stream()->handler_url());
  params.frame_tree_node_id = extension_host_frame_tree_node_id;
  params.source_site_instance = source_site_instance;
  web_contents()->GetController().LoadURLWithParams(params);
}

extensions::StreamInfo* MimeHandlerStreamManager::GetClaimedStreamInfo(
    const content::RenderFrameHost* embedder_host) {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  return iter != stream_infos_.end() ? iter->second.get() : nullptr;
}

const extensions::StreamInfo* MimeHandlerStreamManager::GetClaimedStreamInfo(
    const content::RenderFrameHost* embedder_host) const {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  return iter != stream_infos_.end() ? iter->second.get() : nullptr;
}

extensions::StreamInfo*
MimeHandlerStreamManager::GetClaimedStreamInfoFromContentNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsContentFrameNavigation(navigation_handle)) {
    return nullptr;
  }

  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromContentNavigation(navigation_handle);
  CHECK(embedder_host);

  return GetClaimedStreamInfo(embedder_host);
}

extensions::StreamInfo* MimeHandlerStreamManager::ClaimStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto unclaimed_embedder_info =
      GetUnclaimedEmbedderHostInfo(embedder_host->GetFrameTreeNodeId());
  auto iter = stream_infos_.find(unclaimed_embedder_info);
  CHECK(iter != stream_infos_.end());

  extensions::StreamInfo* stream_info = iter->second.get();

  auto claimed_embedder_info = GetEmbedderHostInfo(embedder_host);
  stream_infos_[claimed_embedder_info] = std::move(iter->second);
  stream_infos_.erase(iter);

  return stream_info;
}

void MimeHandlerStreamManager::DeleteClaimedStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  CHECK(iter != stream_infos_.end());

  extensions::StreamInfo* stream_info = iter->second.get();
  if (stream_info->mime_handler_view_container_manager()) {
    stream_info->mime_handler_view_container_manager()->DestroyFrameContainer(
        stream_info->instance_id());
  }

  stream_infos_.erase(iter);

  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

bool MimeHandlerStreamManager::MaybeDeleteStreamOnExtensionHostChanged(
    content::RenderFrameHost* old_host) {
  if (!IsExtensionHost(old_host)) {
    return false;
  }

  // The initial RFH for the extension frame may commit an about:blank URL.
  // Don't delete the stream when this RFH changes. Another RFH will be chosen
  // to host the extension, with the handler extension URL.
  if (old_host->GetLastCommittedURL().IsAboutBlank()) {
    return false;
  }

  content::RenderFrameHost* embedder_host = old_host->GetParent();
  CHECK(embedder_host);

  DeleteClaimedStreamInfo(embedder_host);
  // DO NOT add code past this point. `this` may have been deleted.

  return true;
}

bool MimeHandlerStreamManager::MaybeDeleteStreamOnContentHostChanged(
    content::RenderFrameHost* old_host) {
  if (!IsContentHost(old_host)) {
    return false;
  }

  // `IsContentHost()` validated: parent is extension host, grandparent is
  // embedder.
  content::RenderFrameHost* embedder_host = old_host->GetParent()->GetParent();
  CHECK(embedder_host);
  auto* stream_info = GetClaimedStreamInfo(embedder_host);

  // Let the delegate validate content-frame invariants.
  stream_info->delegate()->ValidateContentFrameHost(old_host, stream_info);

  // The initial RFH for the content frame is created for the navigation to the
  // stream URL. This navigation may be canceled and never commits. The initial
  // RFH and the actual content RFH have the same frame tree node ID, but the
  // actual content RFH commits its navigation to the original URL. Don't delete
  // the stream when the initial RFH changes.
  const GURL& url = old_host->GetLastCommittedURL();
  if (url.is_empty()) {
    return false;
  }
  CHECK(url == stream_info->stream()->original_url());

  DeleteClaimedStreamInfo(embedder_host);
  // DO NOT add code past this point. `this` may have been deleted.

  return true;
}

bool MimeHandlerStreamManager::MaybeRegisterPdfSubresourceOverride(
    content::NavigationHandle* navigation_handle) {
  // Only register the subresource override if `navigation_handle` is for the
  // PDF content frame. Ignore all other navigations in different frames, such
  // as navigations in the embedder frame or PDF extension frame.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromContentNavigation(navigation_handle);
  if (!claimed_stream_info) {
    return false;
  }

  navigation_handle->RegisterSubresourceOverride(
      claimed_stream_info->stream()->TakeTransferrableURLLoader());

  return true;
}

bool MimeHandlerStreamManager::MaybeSetUpPostMessage(
    content::NavigationHandle* navigation_handle) {
  // Only set up postMessage if `navigation_handle` is for a content frame.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromContentNavigation(navigation_handle);
  if (!claimed_stream_info) {
    return false;
  }

  // If the user reloads the PDF URL before the PDF content frame finishes
  // loading, the initial PDF content frame navigation might reach
  // `MaybeSetUpPostMessage()` during the new PDF load and incorrectly set up
  // the stream info. Only continue PDF setup if the PDF content navigation is
  // for the new PDF load.
  if (navigation_handle->GetFrameTreeNodeId() !=
      claimed_stream_info->content_host_frame_tree_node_id()) {
    return false;
  }

  // Depending on the delegate implementation, the delegate may decide not to
  // opt in to postMessage setup.
  if (!claimed_stream_info->delegate()->ShouldSetUpPostMessage()) {
    return false;
  }

  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromContentNavigation(navigation_handle);
  CHECK(embedder_host);

  // If `owner_type` is kEmbed or kObject, then the PDF is embedded onto another
  // HTML page. `container_host` should be the PDF embedder host's parent.
  // Otherwise, the PDF is full-page, in which `container_host` should be the
  // PDF embedder host itself.
  auto owner_type = embedder_host->GetFrameOwnerElementType();
  bool is_full_page = owner_type != blink::FrameOwnerElementType::kEmbed &&
                      owner_type != blink::FrameOwnerElementType::kObject;
  auto* container_host =
      is_full_page ? embedder_host : embedder_host->GetParent();
  CHECK(container_host);

  auto container_manager = GetMimeHandlerViewContainerManager(container_host);

  // Set up beforeunload support for full page PDF viewer, which will also help
  // set up postMessage support.
  if (is_full_page) {
    container_manager->CreateBeforeUnloadControl(
        base::BindOnce(&MimeHandlerStreamManager::SetUpBeforeUnloadControl,
                       weak_factory_.GetWeakPtr()));
  }

  // Enable postMessage support.
  // The first parameter for DidLoad() is
  // mime_handler_view_guest_element_instance_id, which is used to identify and
  // delete `extensions::MimeHandlerViewFrameContainer` objects. However, OOPIF
  // PDF viewer doesn't have a guest element instance ID. Use the instance ID
  // instead, which is a unique ID for `StreamInfo`.
  container_manager->DidLoad(claimed_stream_info->instance_id(),
                             claimed_stream_info->stream()->original_url());
  claimed_stream_info->set_mime_handler_view_container_manager(
      std::move(container_manager));

  // Hand off to the delegate for any post-setup work.
  claimed_stream_info->delegate()->OnPostMessageSetUp(embedder_host);

  return true;
}

void MimeHandlerStreamManager::SetStreamContentHostFrameTreeNodeId(
    content::NavigationHandle* navigation_handle) {
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromContentNavigation(navigation_handle);
  CHECK(claimed_stream_info);
  claimed_stream_info->set_content_host_frame_tree_node_id(
      navigation_handle->GetFrameTreeNodeId());
}

void MimeHandlerStreamManager::SetUpBeforeUnloadControl(
    mojo::PendingRemote<extensions::mime_handler::BeforeUnloadControl>
        before_unload_control_remote) {
  // TODO(crbug.com/40268279): Currently a no-op. Support the beforeunload API.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MimeHandlerStreamManager);

}  // namespace extensions::mime_handler
