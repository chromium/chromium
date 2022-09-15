// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_

#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"

namespace content {

class RenderFrameHost;

// This class is similiar to PopupMenuHelperMac but positions the popup relative
// to the embedder, and issues a reply to the guest.
// TODO(533069): This class no longer appears to serve a purpose. The base
// PopupMenuHelper already handles the coordinate transformations correctly.
class BrowserPluginPopupMenuHelper : public PopupMenuHelper,
                                     public PopupMenuHelper::Delegate {
 public:
  // Creates a BrowserPluginPopupMenuHelper that positions popups relative to
  // the embedder of `guest_rfh` and will notify `guest_rfh` when a user
  // selects or cancels the popup.
  BrowserPluginPopupMenuHelper(
      RenderFrameHost* guest_rfh,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client);

  BrowserPluginPopupMenuHelper(const BrowserPluginPopupMenuHelper&) = delete;
  BrowserPluginPopupMenuHelper& operator=(const BrowserPluginPopupMenuHelper&) =
      delete;

 private:
  // PopupMenuHelper:Delegate:
  void OnMenuClosed() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
