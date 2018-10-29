// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/public/browser/file_select_listener.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

bool RenderFrameHostDelegate::OnMessageReceived(
    RenderFrameHostImpl* render_frame_host,
    const IPC::Message& message) {
  return false;
}

const GURL& RenderFrameHostDelegate::GetMainFrameLastCommittedURL() const {
  return GURL::EmptyGURL();
}

bool RenderFrameHostDelegate::DidAddMessageToConsole(
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return false;
}

void RenderFrameHostDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    std::unique_ptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
}

WebContents* RenderFrameHostDelegate::GetAsWebContents() {
  return nullptr;
}

InterstitialPage* RenderFrameHostDelegate::GetAsInterstitialPage() {
  return nullptr;
}

void RenderFrameHostDelegate::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    MediaResponseCallback callback) {
  LOG(ERROR) << "RenderFrameHostDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  std::move(callback).Run(MediaStreamDevices(), MEDIA_DEVICE_NOT_SUPPORTED,
                          std::unique_ptr<MediaStreamUI>());
}

bool RenderFrameHostDelegate::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    MediaStreamType type) {
  LOG(ERROR) << "RenderFrameHostDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string RenderFrameHostDelegate::GetDefaultMediaDeviceID(
    MediaStreamType type) {
  return std::string();
}

ui::AXMode RenderFrameHostDelegate::GetAccessibilityMode() const {
  return ui::AXMode();
}

RenderFrameHost* RenderFrameHostDelegate::GetGuestByInstanceID(
    RenderFrameHost* render_frame_host,
    int browser_plugin_instance_id) {
  return nullptr;
}

device::mojom::GeolocationContext*
RenderFrameHostDelegate::GetGeolocationContext() {
  return nullptr;
}

device::mojom::WakeLock* RenderFrameHostDelegate::GetRendererWakeLock() {
  return nullptr;
}

#if defined(OS_ANDROID)
void RenderFrameHostDelegate::GetNFC(device::mojom::NFCRequest request) {}
#endif

bool RenderFrameHostDelegate::ShouldRouteMessageEvent(
    RenderFrameHost* target_rfh,
    SiteInstance* source_site_instance) const {
  return false;
}

RenderFrameHost*
RenderFrameHostDelegate::GetFocusedFrameIncludingInnerWebContents() {
  return nullptr;
}

std::unique_ptr<WebUIImpl>
RenderFrameHostDelegate::CreateWebUIForRenderFrameHost(const GURL& url) {
  return nullptr;
}

bool RenderFrameHostDelegate::ShouldAllowRunningInsecureContent(
    WebContents* web_contents,
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

bool RenderFrameHostDelegate::IsBeingDestroyed() const {
  return false;
}

Visibility RenderFrameHostDelegate::GetVisibility() const {
  return Visibility::HIDDEN;
}

ukm::SourceId RenderFrameHostDelegate::GetUkmSourceIdForLastCommittedSource()
    const {
  return ukm::kInvalidSourceId;
}

}  // namespace content
