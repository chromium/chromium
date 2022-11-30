// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_RESPONSE_INFO_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_RESPONSE_INFO_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net {

class HttpResponseHeaders;

struct NET_EXPORT WebSocketHandshakeResponseInfo {
  WebSocketHandshakeResponseInfo(const GURL& url,
                                 scoped_refptr<HttpResponseHeaders> headers,
                                 const IPEndPoint& remote_endpoint,
                                 base::Time response_time);

  WebSocketHandshakeResponseInfo(const WebSocketHandshakeResponseInfo&) =
      delete;
  WebSocketHandshakeResponseInfo& operator=(
      const WebSocketHandshakeResponseInfo&) = delete;

  ~WebSocketHandshakeResponseInfo();

  // The request URL
  GURL url;
  // HTTP response headers
  scoped_refptr<HttpResponseHeaders> headers;
  // Remote address of the socket.
  IPEndPoint remote_endpoint;
  // The time that this response arrived
  base::Time response_time;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_RESPONSE_INFO_H_
