// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_

#include "base/macros.h"
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;

// This class is similiar to PopupMenuHelperMac but positions the popup relative
// to the embedder, and issues a reply to the guest.
class BrowserPluginPopupMenuHelper : public PopupMenuHelper,
                                     public PopupMenuHelper::Delegate {
 public:
  // Creates a BrowserPluginPopupMenuHelper that positions popups relative to
  // |embedder_rfh| and will notify |guest_rfh| when a user selects or cancels
  // the popup.
  BrowserPluginPopupMenuHelper(
      RenderFrameHostImpl* embedder_rfh,
      RenderFrameHost* guest_rfh,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client);

 private:
  // PopupMenuHelper:
  RenderWidgetHostViewMac* GetRenderWidgetHostView() const override;

  // PopupMenuHelper:Delegate:
  void OnMenuClosed() override;

  RenderFrameHostImpl* embedder_rfh_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPopupMenuHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
