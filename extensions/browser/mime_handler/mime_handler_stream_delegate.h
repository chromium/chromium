// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_

namespace content {
class NavigationHandle;
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

  // Called once the MIME handler extension frame has committed its
  // navigation to the claimed stream's handler URL. Gives the delegate a
  // chance to perform any post-commit setup that needs the just-committed
  // extension host, such as attaching per-frame services. Neither
  // `navigation_handle` nor `stream_info` may be null, and neither pointer is
  // retained past the call. Default implementation is a no-op.
  virtual void OnExtensionFrameFinished(
      content::NavigationHandle* navigation_handle,
      StreamInfo* stream_info);

  // Returns true if the manager should wire up MimeHandlerViewContainerManager
  // postMessage support for this content-frame navigation. Default: false.
  virtual bool ShouldSetUpPostMessage() const;
  // Called after the manager has wired up postMessage support. Only invoked
  // when ShouldSetUpPostMessage() returned true. Default: no-op.
  virtual void OnPostMessageSetUp(content::RenderFrameHost* embedder_host);

  virtual bool PluginCanSave() const;
  virtual void SetPluginCanSave(bool plugin_can_save);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
