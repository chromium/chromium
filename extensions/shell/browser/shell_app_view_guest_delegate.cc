// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_view_guest_delegate.h"

#include "extensions/shell/browser/shell_app_delegate.h"

namespace extensions {

ShellAppViewGuestDelegate::ShellAppViewGuestDelegate() = default;

ShellAppViewGuestDelegate::~ShellAppViewGuestDelegate() = default;

bool ShellAppViewGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Eat the context menu request, as AppShell doesn't show context menus.
  return true;
}

AppDelegate* ShellAppViewGuestDelegate::CreateAppDelegate(
    content::BrowserContext* browser_context) {
  return new ShellAppDelegate();
}

}  // namespace extensions
