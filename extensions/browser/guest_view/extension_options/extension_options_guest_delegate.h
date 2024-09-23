// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace content {
struct ContextMenuParams;
struct OpenURLParams;
class RenderFrameHost;
class WebContents;
class NavigationHandle;
}  // namespace content

namespace extensions {

class ExtensionOptionsGuest;

// Interface to handle communication between ExtensionOptionsGuest (in
// extensions) with the browser.
class ExtensionOptionsGuestDelegate {
 public:
  explicit ExtensionOptionsGuestDelegate(ExtensionOptionsGuest* guest);

  ExtensionOptionsGuestDelegate(const ExtensionOptionsGuestDelegate&) = delete;
  ExtensionOptionsGuestDelegate& operator=(
      const ExtensionOptionsGuestDelegate&) = delete;

  virtual ~ExtensionOptionsGuestDelegate();

  // Shows the context menu for the guest.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  //
  // Returns true if the context menu was handled.
  virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                 const content::ContextMenuParams& params) = 0;

  virtual content::WebContents* OpenURLInNewTab(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) = 0;

  ExtensionOptionsGuest* extension_options_guest() const { return guest_; }

 private:
  const raw_ptr<ExtensionOptionsGuest> guest_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_DELEGATE_H_
