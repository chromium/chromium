// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_delegate.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

bool RenderFrameHostDelegate::OnMessageReceived(
    RenderFrameHostImpl* render_frame_host,
    const IPC::Message& message) {
  return false;
}

const GURL& RenderFrameHostDelegate::GetMainFrameLastCommittedURL() {
  return GURL::EmptyGURL();
}

bool RenderFrameHostDelegate::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return false;
}

WebContents* RenderFrameHostDelegate::GetAsWebContents() {
  return nullptr;
}

void RenderFrameHostDelegate::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    MediaResponseCallback callback) {
  LOG(ERROR) << "RenderFrameHostDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  std::move(callback).Run(blink::MediaStreamDevices(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          std::unique_ptr<MediaStreamUI>());
}

bool RenderFrameHostDelegate::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  LOG(ERROR) << "RenderFrameHostDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string RenderFrameHostDelegate::GetDefaultMediaDeviceID(
    blink::mojom::MediaStreamType type) {
  return std::string();
}

ui::AXMode RenderFrameHostDelegate::GetAccessibilityMode() {
  return ui::AXMode();
}

device::mojom::GeolocationContext*
RenderFrameHostDelegate::GetGeolocationContext() {
  return nullptr;
}

#if defined(OS_ANDROID)
void RenderFrameHostDelegate::GetNFC(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<device::mojom::NFC> receiver) {}
#endif

bool RenderFrameHostDelegate::CanEnterFullscreenMode() {
  return true;
}

bool RenderFrameHostDelegate::ShouldRouteMessageEvent(
    RenderFrameHost* target_rfh,
    SiteInstance* source_site_instance) const {
  return false;
}

RenderFrameHost*
RenderFrameHostDelegate::GetFocusedFrameIncludingInnerWebContents() {
  return nullptr;
}

RenderFrameHostImpl* RenderFrameHostDelegate::GetMainFrame() {
  return nullptr;
}

std::unique_ptr<WebUIImpl>
RenderFrameHostDelegate::CreateWebUIForRenderFrameHost(
    RenderFrameHost* frame_host,
    const GURL& url) {
  return nullptr;
}

RenderFrameHostDelegate* RenderFrameHostDelegate::CreateNewWindow(
    RenderFrameHost* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
}

bool RenderFrameHostDelegate::ShouldAllowRunningInsecureContent(
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  return false;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostDelegate::GetJavaRenderFrameHostDelegate() {
  return nullptr;
}
#endif

bool RenderFrameHostDelegate::IsBeingDestroyed() {
  return false;
}

Visibility RenderFrameHostDelegate::GetVisibility() {
  return Visibility::HIDDEN;
}

ukm::SourceId RenderFrameHostDelegate::
    GetUkmSourceIdForLastCommittedSourceIncludingSameDocument() const {
  return ukm::kInvalidSourceId;
}

RenderFrameHostImpl* RenderFrameHostDelegate::GetMainFrameForInnerDelegate(
    FrameTreeNode* frame_tree_node) {
  return nullptr;
}

media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
RenderFrameHostDelegate::GetRecordAggregateWatchTimeCallback() {
  return base::NullCallback();
}

bool RenderFrameHostDelegate::IsFrameLowPriority(
    const RenderFrameHost* render_frame_host) {
  return false;
}

void RenderFrameHostDelegate::IsClipboardPasteAllowed(
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
  std::move(callback).Run(ClipboardPasteAllowed(true));
}

bool RenderFrameHostDelegate::HasSeenRecentScreenOrientationChange() {
  return false;
}

bool RenderFrameHostDelegate::IsTransientAllowFullscreenActive() const {
  return false;
}

bool RenderFrameHostDelegate::ShowPopupMenu(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr>* menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  return false;
}

}  // namespace content
