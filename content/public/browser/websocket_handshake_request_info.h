// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBSOCKET_HANDSHAKE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_WEBSOCKET_HANDSHAKE_REQUEST_INFO_H_

#include "content/common/content_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

// WebSocketHandshakeRequestInfo provides additional information attached to
// a net::URLRequest.
class WebSocketHandshakeRequestInfo {
 public:
  virtual ~WebSocketHandshakeRequestInfo() {}
  // Returns the ID of the child process where the WebSocket connection lives.
  virtual int GetChildId() = 0;
  // Returns the ID of the renderer frame where the WebSocket connection lives.
  virtual int GetRenderFrameId() = 0;

  // Returns the WebSocketHandshakeRequestInfo instance attached to the given
  // URLRequest. Returns nullptr when no instance is attached.
  CONTENT_EXPORT static WebSocketHandshakeRequestInfo* ForRequest(
      const net::URLRequest* request);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBSOCKET_HANDSHAKE_REQUEST_INFO_H_
