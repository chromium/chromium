// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

class GURL;

namespace extensions {

class ShellWebViewGuestDelegate : public WebViewGuestDelegate {
 public:
  ShellWebViewGuestDelegate();

  ShellWebViewGuestDelegate(const ShellWebViewGuestDelegate&) = delete;
  ShellWebViewGuestDelegate& operator=(const ShellWebViewGuestDelegate&) =
      delete;

  ~ShellWebViewGuestDelegate() override;

  // WebViewGuestDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  void OnShowContextMenu(int request_id) override;
  bool NavigateToURLShouldBlock(const GURL& url) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_WEB_VIEW_GUEST_DELEGATE_H_
