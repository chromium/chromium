// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"

namespace content {

BrowserPluginPopupMenuHelper::BrowserPluginPopupMenuHelper(
    RenderFrameHost* guest_rfh,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client)
    : PopupMenuHelper(this, guest_rfh, std::move(popup_client)) {}

void BrowserPluginPopupMenuHelper::OnMenuClosed() {
  // BrowserPluginGuest doesn't support cancellation of popup menus, so the
  // MenuHelper is its own delegate and OnMenuClosed() is ignored.
}

}  // namespace content
