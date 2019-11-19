// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

WebContents* BrowserPluginGuestDelegate::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  NOTREACHED();
  return nullptr;
}

WebContents* BrowserPluginGuestDelegate::GetOwnerWebContents() {
  return nullptr;
}

bool BrowserPluginGuestDelegate::CanUseCrossProcessFrames() {
  return true;
}

bool BrowserPluginGuestDelegate::CanBeEmbeddedInsideCrossProcessFrames() {
  return false;
}

RenderWidgetHost* BrowserPluginGuestDelegate::GetOwnerRenderWidgetHost() {
  return nullptr;
}

SiteInstance* BrowserPluginGuestDelegate::GetOwnerSiteInstance() {
  return nullptr;
}

RenderFrameHost* BrowserPluginGuestDelegate::GetEmbedderFrame() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
