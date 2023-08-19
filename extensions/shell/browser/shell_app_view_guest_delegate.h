// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_VIEW_GUEST_DELEGATE_H_

#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/guest_view/app_view/app_view_guest_delegate.h"

namespace extensions {

class ShellAppViewGuestDelegate : public AppViewGuestDelegate {
 public:
  ShellAppViewGuestDelegate();

  ShellAppViewGuestDelegate(const ShellAppViewGuestDelegate&) = delete;
  ShellAppViewGuestDelegate& operator=(const ShellAppViewGuestDelegate&) =
      delete;

  ~ShellAppViewGuestDelegate() override;

  // AppViewGuestDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  AppDelegate* CreateAppDelegate(
      content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_VIEW_GUEST_DELEGATE_H_
