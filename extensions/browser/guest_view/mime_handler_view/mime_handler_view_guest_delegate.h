// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include <string>

#include "base/macros.h"

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace extensions {

// A delegate class of MimeHandlerViewGuest that are not a part of chrome.
class MimeHandlerViewGuestDelegate {
 public:
  MimeHandlerViewGuestDelegate() {}
  virtual ~MimeHandlerViewGuestDelegate() {}

  // Handles context menu, or returns false if unhandled.
  virtual bool HandleContextMenu(content::WebContents* web_contents,
                                 const content::ContextMenuParams& params);
  // Called when MimeHandlerViewGuest has an associated embedder frame.
  virtual void RecordLoadMetric(bool in_main_frame,
                                const std::string& mime_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuestDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
