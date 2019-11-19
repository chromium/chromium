// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_KEY_H_
#define NET_SPDY_SPDY_SESSION_KEY_H_

#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/socket/socket_tag.h"

namespace net {

// SpdySessionKey is used as unique index for SpdySessionPool.
class NET_EXPORT_PRIVATE SpdySessionKey {
 public:
  enum class IsProxySession {
    kFalse,
    // This means this is a ProxyServer::Direct() session for an HTTP2 proxy,
    // with |host_port_pair| being the proxy host and port. This should not be
    // confused with a tunnel over an HTTP2 proxy session, for which
    // |proxy_server| will be information about the proxy being used, and
    // |host_port_pair| will be information not about the proxy, but the host
    // that we're proxying the connection to.
    kTrue,
  };
  SpdySessionKey();

  SpdySessionKey(const HostPortPair& host_port_pair,
                 const ProxyServer& proxy_server,
                 PrivacyMode privacy_mode,
                 IsProxySession is_proxy_session,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 bool disable_secure_dns);

  SpdySessionKey(const SpdySessionKey& other);

  ~SpdySessionKey();

  // Comparator function so this can be placed in a std::map.
  bool operator<(const SpdySessionKey& other) const;

  // Equality tests of contents.
  bool operator==(const SpdySessionKey& other) const;
  bool operator!=(const SpdySessionKey& other) const;

  const HostPortProxyPair& host_port_proxy_pair() const {
    return host_port_proxy_pair_;
  }

  const HostPortPair& host_port_pair() const {
    return host_port_proxy_pair_.first;
  }

  const ProxyServer& proxy_server() const {
    return host_port_proxy_pair_.second;
  }

  PrivacyMode privacy_mode() const {
    return privacy_mode_;
  }

  IsProxySession is_proxy_session() const { return is_proxy_session_; }

  const SocketTag& socket_tag() const { return socket_tag_; }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  bool disable_secure_dns() const { return disable_secure_dns_; }

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  HostPortProxyPair host_port_proxy_pair_;
  // If enabled, then session cannot be tracked by the server.
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  IsProxySession is_proxy_session_;
  SocketTag socket_tag_;
  // Used to separate requests made in different contexts.
  NetworkIsolationKey network_isolation_key_;
  bool disable_secure_dns_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_KEY_H_
