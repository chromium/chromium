// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_

namespace content {
struct ContextMenuParams;
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {
class AppDelegate;

// Interface to handle communication between AppView (in extensions) with the
// browser.
class AppViewGuestDelegate {
 public:
  virtual ~AppViewGuestDelegate();

  // Shows the context menu for the guest.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  //
  // Returns true if the context menu was handled.
  virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                 const content::ContextMenuParams& params) = 0;

  // Returns an AppDelegate to be used by the AppViewGuest.
  virtual AppDelegate* CreateAppDelegate(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_DELEGATE_H_
