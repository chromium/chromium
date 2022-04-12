// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_KEY_H_
#define NET_QUIC_QUIC_SESSION_KEY_H_

#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"

namespace net {

// The key used to identify sessions. Includes the quic::QuicServerId and socket
// tag.
class QUIC_EXPORT_PRIVATE QuicSessionKey {
 public:
  QuicSessionKey();
  QuicSessionKey(const HostPortPair& host_port_pair,
                 PrivacyMode privacy_mode,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 SecureDnsPolicy secure_dns_policy);
  QuicSessionKey(const std::string& host,
                 uint16_t port,
                 PrivacyMode privacy_mode,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 SecureDnsPolicy secure_dns_policy);
  QuicSessionKey(const quic::QuicServerId& server_id,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 SecureDnsPolicy secure_dns_policy);
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

  PrivacyMode privacy_mode() const {
    return server_id_.privacy_mode_enabled() ? PRIVACY_MODE_ENABLED
                                             : PRIVACY_MODE_DISABLED;
  }

  const quic::QuicServerId& server_id() const { return server_id_; }

  SocketTag socket_tag() const { return socket_tag_; }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }

 private:
  quic::QuicServerId server_id_;
  SocketTag socket_tag_;
  // Used to separate requests made in different contexts.
  NetworkIsolationKey network_isolation_key_;
  SecureDnsPolicy secure_dns_policy_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_KEY_H_
