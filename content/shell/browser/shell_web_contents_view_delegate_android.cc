// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_web_contents_view_delegate.h"

#include "base/command_line.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"

namespace content {

WebContentsViewDelegate* CreateShellWebContentsViewDelegate(
    WebContents* web_contents) {
  return new ShellWebContentsViewDelegate(web_contents);
}


ShellWebContentsViewDelegate::ShellWebContentsViewDelegate(
    WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);  // Avoids 'unused private field' build error.
}

ShellWebContentsViewDelegate::~ShellWebContentsViewDelegate() {
}

void ShellWebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {}

}  // namespace content
