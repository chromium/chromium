// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_KEY_H_
#define NET_SPDY_SPDY_SESSION_KEY_H_

#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
namespace net {

// SpdySessionKey is used as unique index for SpdySessionPool.
class NET_EXPORT_PRIVATE SpdySessionKey {
 public:
  enum class IsProxySession {
    kFalse,
    // This means this is a ProxyServer::Direct() session for an HTTP2 proxy,
    // with |host_port_pair| being the proxy host and port. This should not be
    // confused with a tunnel over an HTTP2 proxy session, for which
    // |proxy_chain| will be information about the proxy being used, and
    // |host_port_pair| will be information not about the proxy, but the host
    // that we're proxying the connection to.
    kTrue,
  };

  SpdySessionKey();

  SpdySessionKey(const HostPortPair& host_port_pair,
                 const ProxyChain& proxy_chain,
                 PrivacyMode privacy_mode,
                 IsProxySession is_proxy_session,
                 const SocketTag& socket_tag,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 SecureDnsPolicy secure_dns_policy);

  SpdySessionKey(const SpdySessionKey& other);

  ~SpdySessionKey();

  // Comparator function so this can be placed in a std::map.
  bool operator<(const SpdySessionKey& other) const;

  // Equality tests of contents.
  bool operator==(const SpdySessionKey& other) const;
  bool operator!=(const SpdySessionKey& other) const;

  // Struct returned by CompareForAliasing().
  struct CompareForAliasingResult {
    // True if the two SpdySessionKeys match, except possibly for their
    // |host_port_pair| and |socket_tag|.
    bool is_potentially_aliasable = false;
    // True if the |socket_tag| fields match. If this is false, it's up to the
    // caller to change the tag of the session (if possible) or to not alias the
    // session, even if |is_potentially_aliasable| is true.
    bool is_socket_tag_match = false;
  };

  // Checks if requests using SpdySessionKey can potentially be used to service
  // requests using another. The caller *MUST* also make sure that the session
  // associated with one key has been verified for use with the other.
  //
  // Note that this method is symmetric, so it doesn't matter which key's method
  // is called on the other.
  CompareForAliasingResult CompareForAliasing(
      const SpdySessionKey& other) const;

  const HostPortProxyPair& host_port_proxy_pair() const {
    return host_port_proxy_pair_;
  }

  const HostPortPair& host_port_pair() const {
    return host_port_proxy_pair_.first;
  }

  const ProxyChain& proxy_chain() const { return host_port_proxy_pair_.second; }

  PrivacyMode privacy_mode() const {
    return privacy_mode_;
  }

  IsProxySession is_proxy_session() const { return is_proxy_session_; }

  const SocketTag& socket_tag() const { return socket_tag_; }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }

 private:
  HostPortProxyPair host_port_proxy_pair_;
  // If enabled, then session cannot be tracked by the server.
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  IsProxySession is_proxy_session_;
  SocketTag socket_tag_;
  // Used to separate requests made in different contexts. If network state
  // partitioning is disabled this will be set to an empty key.
  NetworkAnonymizationKey network_anonymization_key_;
  SecureDnsPolicy secure_dns_policy_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_KEY_H_
