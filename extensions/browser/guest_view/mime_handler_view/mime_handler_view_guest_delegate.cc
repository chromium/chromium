// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"

namespace extensions {

bool MimeHandlerViewGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

void MimeHandlerViewGuestDelegate::RecordLoadMetric(
    bool is_full_page,
    const std::string& mime_type,
    content::BrowserContext* browser_context) {}

}  // namespace extensions
