// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_GENERIC_MIME_HANDLER_STREAM_DELEGATE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_GENERIC_MIME_HANDLER_STREAM_DELEGATE_H_

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"

namespace extensions {

class StreamInfo;

namespace mime_handler {

// Stream delegate for generic MIME handler extensions. Unlike the OOPIF
// PDF viewer -- which renders a 3-level frame tree (embedder -> extension
// frame -> content frame hosting the PDF plugin) -- generic MIME handlers
// use a 2-level frame tree (embedder -> extension frame). The extension
// page itself consumes the intercepted response via the stream URL, so
// there is no separate "content frame" to validate or wire postMessage
// through. As a result, `ValidateContentFrameHost()` is a no-op and
// `OnExtensionFrameReadyToCommit()` registers the transferable loader
// as a subresource override on the extension frame directly.
class GenericMimeHandlerStreamDelegate : public MimeHandlerStreamDelegate {
 public:
  GenericMimeHandlerStreamDelegate();

  GenericMimeHandlerStreamDelegate(const GenericMimeHandlerStreamDelegate&) =
      delete;
  GenericMimeHandlerStreamDelegate& operator=(
      const GenericMimeHandlerStreamDelegate&) = delete;

  // MimeHandlerStreamDelegate:
  void OnExtensionFrameReadyToCommit(
      content::NavigationHandle* navigation_handle,
      extensions::StreamInfo* stream_info) override;
  void ValidateContentFrameHost(content::RenderFrameHost* content_host,
                                extensions::StreamInfo* stream_info) override;

  ~GenericMimeHandlerStreamDelegate() override;
};

}  // namespace mime_handler

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_GENERIC_MIME_HANDLER_STREAM_DELEGATE_H_
