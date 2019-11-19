// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"

namespace extensions {

bool MimeHandlerViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  return false;
}

void MimeHandlerViewGuestDelegate::RecordLoadMetric(
    bool in_main_frame,
    const std::string& mime_type) {}

}  // namespace extensions
