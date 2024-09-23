// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_ALIAS_KEY_H_
#define NET_QUIC_QUIC_SESSION_ALIAS_KEY_H_

#include "net/base/net_export.h"
#include "net/quic/quic_session_key.h"
#include "url/scheme_host_port.h"

namespace net {

// This class encompasses `destination()` and `server_id()`.
// `destination()` is a SchemeHostPort which is resolved
// and a quic::QuicConnection is made to the resulting IP address.
// `server_id()` identifies the origin of the request,
// the crypto handshake advertises `server_id().host()` to the server,
// and the certificate is also matched against `server_id().host()`.
class NET_EXPORT_PRIVATE QuicSessionAliasKey {
 public:
  QuicSessionAliasKey() = default;
  QuicSessionAliasKey(url::SchemeHostPort destination,
                      QuicSessionKey session_key);
  ~QuicSessionAliasKey() = default;

  QuicSessionAliasKey(const QuicSessionAliasKey& other) = default;
  QuicSessionAliasKey& operator=(const QuicSessionAliasKey& other) = default;

  QuicSessionAliasKey(QuicSessionAliasKey&& other) = default;
  QuicSessionAliasKey& operator=(QuicSessionAliasKey&& other) = default;

  // Needed to be an element of std::set.
  bool operator<(const QuicSessionAliasKey& other) const;
  bool operator==(const QuicSessionAliasKey& other) const;

  const url::SchemeHostPort& destination() const { return destination_; }
  const quic::QuicServerId& server_id() const {
    return session_key_.server_id();
  }
  const QuicSessionKey& session_key() const { return session_key_; }

 private:
  url::SchemeHostPort destination_;
  QuicSessionKey session_key_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_ALIAS_KEY_H_
