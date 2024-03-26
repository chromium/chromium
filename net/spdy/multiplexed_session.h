// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_MULTIPLEXED_SESSION_H_
#define NET_SPDY_MULTIPLEXED_SESSION_H_

#include <string_view>

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_stream.h"
#include "net/ssl/ssl_info.h"

namespace url {
class SchemeHostPort;
}

namespace net {

// Base class for SPDY and QUIC sessions.
class NET_EXPORT_PRIVATE MultiplexedSession {
 public:
  virtual ~MultiplexedSession() = default;

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  virtual bool GetSSLInfo(SSLInfo* ssl_info) const = 0;

  // Gets the remote endpoint of the socket that the HTTP stream is using, if
  // any. Returns OK and fills in |endpoint| if it is available; returns an
  // error and does not modify |endpoint| otherwise.
  virtual int GetRemoteEndpoint(IPEndPoint* endpoint) = 0;

  // The value corresponding to |scheme_host_port| in the ACCEPT_CH frame
  // received during TLS handshake via the ALPS extension, or the empty string
  // if the server did not send one.  Unlike Accept-CH header fields received in
  // HTTP responses, this value is available before any requests are made.
  //
  // Note that this uses url::SchemeHostPort instead of url::Origin because this
  // is based around network authorities, as opposed to general RFC 6454
  // origins.
  virtual std::string_view GetAcceptChViaAlps(
      const url::SchemeHostPort& scheme_host_port) const = 0;
};

// A handle to a multiplexed session which will be valid even after the
// underlying session is deleted.
class NET_EXPORT_PRIVATE MultiplexedSessionHandle {
 public:
  explicit MultiplexedSessionHandle(base::WeakPtr<MultiplexedSession> session);
  virtual ~MultiplexedSessionHandle();

  // Gets the remote endpoint of the socket that the HTTP stream is using, if
  // any. Returns OK and fills in |endpoint| if it is available; returns an
  // error and does not modify |endpoint| otherwise.
  int GetRemoteEndpoint(IPEndPoint* endpoint);

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  bool GetSSLInfo(SSLInfo* ssl_info) const;

  // Caches SSL info from the underlying session.
  void SaveSSLInfo();

  // The value corresponding to |scheme_host_port| in the ACCEPT_CH frame
  // received during TLS handshake via the ALPS extension, or the empty string
  // if the server did not send one or if the underlying session is not
  // available.
  //
  // Note that this uses url::SchemeHostPort instead of url::Origin because this
  // is based around network authorities, as opposed to general RFC 6454
  // origins.
  virtual std::string_view GetAcceptChViaAlps(
      const url::SchemeHostPort& scheme_host_port) const;

 private:
  base::WeakPtr<MultiplexedSession> session_;
  SSLInfo ssl_info_;
  bool has_ssl_info_;
};

}  // namespace net

#endif  // NET_SPDY_MULTIPLEXED_SESSION_H_
