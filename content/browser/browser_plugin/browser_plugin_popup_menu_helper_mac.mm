// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

BrowserPluginPopupMenuHelper::BrowserPluginPopupMenuHelper(
    RenderFrameHostImpl* embedder_rfh,
    RenderFrameHost* guest_rfh,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client)
    : PopupMenuHelper(this, guest_rfh, std::move(popup_client)),
      embedder_rfh_(embedder_rfh) {}

RenderWidgetHostViewMac*
    BrowserPluginPopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(embedder_rfh_->GetView());
}

void BrowserPluginPopupMenuHelper::OnMenuClosed() {
  // BrowserPluginGuest doesn't support cancellation of popup menus, so the
  // MenuHelper is its own delegate and OnMenuClosed() is ignored.
}

}  // namespace content
