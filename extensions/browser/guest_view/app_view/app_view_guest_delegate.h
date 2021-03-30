// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_

namespace content {
struct ContextMenuParams;
class WebContents;
}

namespace extensions {
class AppDelegate;

// Interface to handle communication between AppView (in extensions) with the
// browser.
class AppViewGuestDelegate {
 public:
  virtual ~AppViewGuestDelegate();

  // Shows the context menu for the guest.
  // Returns true if the context menu was handled.
  virtual bool HandleContextMenu(content::WebContents* web_contents,
                                 const content::ContextMenuParams& params) = 0;

  // Returns an AppDelegate to be used by the AppViewGuest.
  virtual AppDelegate* CreateAppDelegate(
      content::WebContents* web_contents) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_
