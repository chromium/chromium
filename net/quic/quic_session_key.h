// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_KEY_H_
#define NET_QUIC_QUIC_SESSION_KEY_H_

#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"

namespace net {

// The key used to identify sessions. Includes the quic::QuicServerId and socket
// tag.
class NET_EXPORT_PRIVATE QuicSessionKey {
 public:
  QuicSessionKey();
  QuicSessionKey(const HostPortPair& host_port_pair,
                 PrivacyMode privacy_mode,
                 const ProxyChain& proxy_chain,
                 SessionUsage session_usage,
                 const SocketTag& socket_tag,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 SecureDnsPolicy secure_dns_policy,
                 bool require_dns_https_alpn);
  QuicSessionKey(const std::string& host,
                 uint16_t port,
                 PrivacyMode privacy_mode,
                 const ProxyChain& proxy_chain,
                 SessionUsage session_usage,
                 const SocketTag& socket_tag,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 SecureDnsPolicy secure_dns_policy,
                 bool require_dns_https_alpn);
  QuicSessionKey(const quic::QuicServerId& server_id,
                 PrivacyMode privacy_mode,
                 const ProxyChain& proxy_chain,
                 SessionUsage session_usage,
                 const SocketTag& socket_tag,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 SecureDnsPolicy secure_dns_policy,
                 bool require_dns_https_alpn);
  QuicSessionKey(const QuicSessionKey& other);
  ~QuicSessionKey() = default;

  // Needed to be an element of std::set.
  bool operator<(const QuicSessionKey& other) const;
  bool operator==(const QuicSessionKey& other) const;

  // Checks if requests using QuicSessionKey can potentially be used to service
  // requests using another.  Returns true if all fields except QuicServerId's
  // host and port match. The caller *MUST* also make sure that the session
  // associated with one key has been verified for use with the host/port of the
  // other.
  //
  // Note that this method is symmetric, so it doesn't matter which key's method
  // is called on the other.
  bool CanUseForAliasing(const QuicSessionKey& other) const;

  const std::string& host() const { return server_id_.host(); }

  PrivacyMode privacy_mode() const { return privacy_mode_; }

  const quic::QuicServerId& server_id() const { return server_id_; }

  const ProxyChain& proxy_chain() const { return proxy_chain_; }

  SessionUsage session_usage() const { return session_usage_; }

  SocketTag socket_tag() const { return socket_tag_; }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }

  bool require_dns_https_alpn() const { return require_dns_https_alpn_; }

 private:
  quic::QuicServerId server_id_;
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  ProxyChain proxy_chain_;
  SessionUsage session_usage_;
  SocketTag socket_tag_;
  // Used to separate requests made in different contexts.
  NetworkAnonymizationKey network_anonymization_key_;
  SecureDnsPolicy secure_dns_policy_;
  bool require_dns_https_alpn_ = false;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_KEY_H_
