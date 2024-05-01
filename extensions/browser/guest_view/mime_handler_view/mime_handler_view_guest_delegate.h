// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include <string>

namespace content {
class BrowserContext;
class RenderFrameHost;
struct ContextMenuParams;
}  // namespace content

namespace extensions {

// A delegate class of MimeHandlerViewGuest that are not a part of chrome.
class MimeHandlerViewGuestDelegate {
 public:
  MimeHandlerViewGuestDelegate() {}

  MimeHandlerViewGuestDelegate(const MimeHandlerViewGuestDelegate&) = delete;
  MimeHandlerViewGuestDelegate& operator=(const MimeHandlerViewGuestDelegate&) =
      delete;

  virtual ~MimeHandlerViewGuestDelegate() {}

  // Handles context menu, or returns false if unhandled.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                 const content::ContextMenuParams& params);
  // Called when MimeHandlerViewGuest has an associated embedder frame.
  virtual void RecordLoadMetric(bool is_full_page,
                                const std::string& mime_type,
                                content::BrowserContext* browser_context);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
