// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_key.h"

#include "base/feature_list.h"
#include "net/base/features.h"
#include "net/dns/public/secure_dns_policy.h"

namespace net {

QuicSessionKey::QuicSessionKey() = default;

QuicSessionKey::QuicSessionKey(
    const HostPortPair& host_port_pair,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : QuicSessionKey(host_port_pair.host(),
                     host_port_pair.port(),
                     privacy_mode,
                     socket_tag,
                     network_anonymization_key,
                     secure_dns_policy,
                     require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(
    const std::string& host,
    uint16_t port,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : QuicSessionKey(
          // TODO(crbug.com/1103350): Handle non-boolean privacy modes.
          quic::QuicServerId(host, port, privacy_mode != PRIVACY_MODE_DISABLED),
          socket_tag,
          network_anonymization_key,
          secure_dns_policy,
          require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(
    const quic::QuicServerId& server_id,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : server_id_(server_id),
      socket_tag_(socket_tag),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? network_anonymization_key
              : NetworkAnonymizationKey()),
      secure_dns_policy_(secure_dns_policy),
      require_dns_https_alpn_(require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(const QuicSessionKey& other) = default;

bool QuicSessionKey::operator<(const QuicSessionKey& other) const {
  return std::tie(server_id_, socket_tag_, network_anonymization_key_,
                  secure_dns_policy_, require_dns_https_alpn_) <
         std::tie(other.server_id_, other.socket_tag_,
                  other.network_anonymization_key_, other.secure_dns_policy_,
                  other.require_dns_https_alpn_);
}
bool QuicSessionKey::operator==(const QuicSessionKey& other) const {
  return server_id_ == other.server_id_ && socket_tag_ == other.socket_tag_ &&
         network_anonymization_key_ == other.network_anonymization_key_ &&
         secure_dns_policy_ == other.secure_dns_policy_ &&
         require_dns_https_alpn_ == other.require_dns_https_alpn_;
}

bool QuicSessionKey::CanUseForAliasing(const QuicSessionKey& other) const {
  return server_id_.privacy_mode_enabled() ==
             other.server_id_.privacy_mode_enabled() &&
         socket_tag_ == other.socket_tag_ &&
         network_anonymization_key_ == other.network_anonymization_key_ &&
         secure_dns_policy_ == other.secure_dns_policy_ &&
         require_dns_https_alpn_ == other.require_dns_https_alpn_;
}

}  // namespace net
