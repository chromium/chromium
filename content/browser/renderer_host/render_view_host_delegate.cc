// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

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

SessionStorageNamespaceMap
RenderViewHostDelegate::GetSessionStorageNamespaceMap() {
  return SessionStorageNamespaceMap();
}

bool RenderViewHostDelegate::IsNeverComposited() {
  return false;
}

bool RenderViewHostDelegate::IsJavaScriptDialogShowing() const {
  return false;
}

bool RenderViewHostDelegate::ShouldIgnoreUnresponsiveRenderer() {
  return false;
}

bool RenderViewHostDelegate::IsPortal() {
  return false;
}

}  // namespace content
