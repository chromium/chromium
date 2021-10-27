// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_

#include "base/callback.h"
#include "components/guest_view/browser/guest_view_base.h"

namespace extensions {

// A delegate class of WebViewGuest that are not a part of chrome.
class WebViewGuestDelegate {
 public :
  virtual ~WebViewGuestDelegate() {}

  // Called when context menu operation was handled.
  virtual bool HandleContextMenu(const content::ContextMenuParams& params) = 0;

  // Shows the context menu for the guest.
  virtual void OnShowContextMenu(int request_id) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
