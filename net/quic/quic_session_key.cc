// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_key.h"

#include <tuple>

#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"

namespace net {

QuicSessionKey::QuicSessionKey() = default;

QuicSessionKey::QuicSessionKey(
    const HostPortPair& host_port_pair,
    PrivacyMode privacy_mode,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : QuicSessionKey(host_port_pair.host(),
                     host_port_pair.port(),
                     privacy_mode,
                     proxy_chain,
                     session_usage,
                     socket_tag,
                     network_anonymization_key,
                     secure_dns_policy,
                     require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(
    const std::string& host,
    uint16_t port,
    PrivacyMode privacy_mode,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : QuicSessionKey(quic::QuicServerId(host, port),
                     privacy_mode,
                     proxy_chain,
                     session_usage,
                     socket_tag,
                     network_anonymization_key,
                     secure_dns_policy,
                     require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn)
    : server_id_(server_id),
      privacy_mode_(privacy_mode),
      proxy_chain_(proxy_chain),
      session_usage_(session_usage),
      socket_tag_(socket_tag),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? network_anonymization_key
              : NetworkAnonymizationKey()),
      secure_dns_policy_(secure_dns_policy),
      require_dns_https_alpn_(require_dns_https_alpn) {}

QuicSessionKey::QuicSessionKey(const QuicSessionKey& other) = default;

bool QuicSessionKey::operator<(const QuicSessionKey& other) const {
  const uint16_t port = server_id_.port();
  const uint16_t other_port = other.server_id_.port();
  return std::tie(port, server_id_.host(), privacy_mode_, proxy_chain_,
                  session_usage_, socket_tag_, network_anonymization_key_,
                  secure_dns_policy_, require_dns_https_alpn_) <
         std::tie(other_port, other.server_id_.host(), other.privacy_mode_,
                  other.proxy_chain_, other.session_usage_, other.socket_tag_,
                  other.network_anonymization_key_, other.secure_dns_policy_,
                  other.require_dns_https_alpn_);
}
bool QuicSessionKey::operator==(const QuicSessionKey& other) const {
  return server_id_.port() == other.server_id_.port() &&
         server_id_.host() == other.server_id_.host() &&
         privacy_mode_ == other.privacy_mode_ &&
         proxy_chain_ == other.proxy_chain_ &&
         session_usage_ == other.session_usage_ &&
         socket_tag_ == other.socket_tag_ &&
         network_anonymization_key_ == other.network_anonymization_key_ &&
         secure_dns_policy_ == other.secure_dns_policy_ &&
         require_dns_https_alpn_ == other.require_dns_https_alpn_;
}

bool QuicSessionKey::CanUseForAliasing(const QuicSessionKey& other) const {
  return privacy_mode_ == other.privacy_mode() &&
         socket_tag_ == other.socket_tag_ &&
         proxy_chain_ == other.proxy_chain_ &&
         session_usage_ == other.session_usage_ &&
         network_anonymization_key_ == other.network_anonymization_key_ &&
         secure_dns_policy_ == other.secure_dns_policy_ &&
         require_dns_https_alpn_ == other.require_dns_https_alpn_;
}

}  // namespace net
