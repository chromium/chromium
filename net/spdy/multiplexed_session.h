// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_MULTIPLEXED_SESSION_H_
#define NET_SPDY_MULTIPLEXED_SESSION_H_

#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_stream.h"
#include "net/ssl/ssl_info.h"

namespace net {

// Base class for SPDY and QUIC sessions.
class NET_EXPORT_PRIVATE MultiplexedSession {
 public:
  virtual ~MultiplexedSession() {}

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  virtual bool GetSSLInfo(SSLInfo* ssl_info) const = 0;

  // Gets the remote endpoint of the socket that the HTTP stream is using, if
  // any. Returns true and fills in |endpoint| if it is available; returns false
  // and does not modify |endpoint| if it is unavailable.
  virtual bool GetRemoteEndpoint(IPEndPoint* endpoint) = 0;
};

// A handle to a multiplexed session which will be valid even after the
// underlying session is deleted.
class NET_EXPORT_PRIVATE MultiplexedSessionHandle {
 public:
  explicit MultiplexedSessionHandle(base::WeakPtr<MultiplexedSession> session);
  virtual ~MultiplexedSessionHandle();

  // Gets the remote endpoint of the socket that the HTTP stream is using, if
  // any. Returns true and fills in |endpoint| if it is available; returns false
  // and does not modify |endpoint| if it is unavailable.
  bool GetRemoteEndpoint(IPEndPoint* endpoint);

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  bool GetSSLInfo(SSLInfo* ssl_info) const;

  // Caches SSL info from the underlying session.
  void SaveSSLInfo();

 private:
  base::WeakPtr<MultiplexedSession> session_;
  SSLInfo ssl_info_;
  bool has_ssl_info_;
};

}  // namespace net

#endif  // NET_SPDY_MULTIPLEXED_SESSION_H_
