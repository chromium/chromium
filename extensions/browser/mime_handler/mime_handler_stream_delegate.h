// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {

class StreamInfo;

class MimeHandlerStreamDelegate {
 public:
  MimeHandlerStreamDelegate();
  MimeHandlerStreamDelegate(const MimeHandlerStreamDelegate&) = delete;
  MimeHandlerStreamDelegate& operator=(const MimeHandlerStreamDelegate&) =
      delete;
  virtual ~MimeHandlerStreamDelegate();

  // Called when `stream_info` has just been claimed by `embedder_host`,
  // before the MIME handler navigates. Gives the delegate a chance to
  // perform any claim-time initialization that needs the embedder
  // frame. Neither `embedder_host` nor `stream_info` may be null, and
  // neither pointer is retained past the call. Default implementation
  // is a no-op.
  virtual void OnStreamClaimed(content::RenderFrameHost* embedder_host,
                               StreamInfo* stream_info);
  virtual bool PluginCanSave() const;
  virtual void SetPluginCanSave(bool plugin_can_save);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
