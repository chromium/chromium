// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_KEY_H_
#define NET_QUIC_QUIC_SESSION_KEY_H_

#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"

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
                 bool disable_secure_dns);
  QuicSessionKey(const std::string& host,
                 uint16_t port,
                 PrivacyMode privacy_mode,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 bool disable_secure_dns);
  QuicSessionKey(const quic::QuicServerId& server_id,
                 const SocketTag& socket_tag,
                 const NetworkIsolationKey& network_isolation_key,
                 bool disable_secure_dns);
  QuicSessionKey(const QuicSessionKey& other);
  ~QuicSessionKey() = default;

  // Needed to be an element of std::set.
  bool operator<(const QuicSessionKey& other) const;
  bool operator==(const QuicSessionKey& other) const;

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

  bool disable_secure_dns() const { return disable_secure_dns_; }

  size_t EstimateMemoryUsage() const;

 private:
  quic::QuicServerId server_id_;
  SocketTag socket_tag_;
  // Used to separate requests made in different contexts.
  NetworkIsolationKey network_isolation_key_;
  bool disable_secure_dns_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SERVER_ID_H_
