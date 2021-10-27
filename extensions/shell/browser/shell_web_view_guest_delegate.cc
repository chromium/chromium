// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_web_view_guest_delegate.h"

namespace extensions {

ShellWebViewGuestDelegate::ShellWebViewGuestDelegate() = default;
ShellWebViewGuestDelegate::~ShellWebViewGuestDelegate() = default;

bool ShellWebViewGuestDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Eat the context menu request, as AppShell doesn't show context menus.
  return true;
}

void ShellWebViewGuestDelegate::OnShowContextMenu(int request_id) {}

}  // namespace extensions
