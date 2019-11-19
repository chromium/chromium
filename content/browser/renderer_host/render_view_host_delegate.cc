// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

#include "content/public/common/web_preferences.h"
#include "url/gurl.h"

namespace content {

RenderViewHostDelegateView* RenderViewHostDelegate::GetDelegateView() {
  return nullptr;
}

bool RenderViewHostDelegate::OnMessageReceived(
    RenderViewHostImpl* render_view_host,
    const IPC::Message& message) {
  return false;
}

WebContents* RenderViewHostDelegate::GetAsWebContents() {
  return nullptr;
}

SessionStorageNamespace* RenderViewHostDelegate::GetSessionStorageNamespace(
    SiteInstance* instance) {
  return nullptr;
}

SessionStorageNamespaceMap
RenderViewHostDelegate::GetSessionStorageNamespaceMap() {
  return SessionStorageNamespaceMap();
}

FrameTree* RenderViewHostDelegate::GetFrameTree() {
  return nullptr;
}

bool RenderViewHostDelegate::IsNeverVisible() {
  return false;
}

bool RenderViewHostDelegate::IsOverridingUserAgent() {
  return false;
}

bool RenderViewHostDelegate::IsJavaScriptDialogShowing() const {
  return false;
}

bool RenderViewHostDelegate::ShouldIgnoreUnresponsiveRenderer() {
  return false;
}

bool RenderViewHostDelegate::HideDownloadUI() const {
  return false;
}

bool RenderViewHostDelegate::HasPersistentVideo() const {
  return false;
}

bool RenderViewHostDelegate::IsSpatialNavigationDisabled() const {
  return false;
}

RenderFrameHostImpl* RenderViewHostDelegate::GetPendingMainFrame() {
  return nullptr;
}

bool RenderViewHostDelegate::IsPortal() const {
  return false;
}

}  // namespace content
