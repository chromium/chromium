// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_key.h"

#include "base/strings/strcat.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/socket_tag.h"
#include "net/spdy/spdy_session_key.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

HttpStreamKey::HttpStreamKey() = default;

HttpStreamKey::HttpStreamKey(url::SchemeHostPort destination,
                             PrivacyMode privacy_mode,
                             SocketTag socket_tag,
                             NetworkAnonymizationKey network_anonymization_key,
                             SecureDnsPolicy secure_dns_policy,
                             bool disable_cert_network_fetches)
    : destination_(std::move(destination)),
      privacy_mode_(privacy_mode),
      socket_tag_(std::move(socket_tag)),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? std::move(network_anonymization_key)
              : NetworkAnonymizationKey()),
      secure_dns_policy_(secure_dns_policy),
      disable_cert_network_fetches_(disable_cert_network_fetches) {
  CHECK(socket_tag_ == SocketTag()) << "Socket tag is not supported yet";
}

HttpStreamKey::~HttpStreamKey() = default;

HttpStreamKey::HttpStreamKey(const HttpStreamKey& other) = default;

HttpStreamKey& HttpStreamKey::operator=(const HttpStreamKey& other) = default;

bool HttpStreamKey::operator==(const HttpStreamKey& other) const = default;

bool HttpStreamKey::operator<(const HttpStreamKey& other) const {
  return std::tie(destination_, privacy_mode_, socket_tag_,
                  network_anonymization_key_, secure_dns_policy_,
                  disable_cert_network_fetches_) <
         std::tie(other.destination_, other.privacy_mode_, other.socket_tag_,
                  other.network_anonymization_key_, other.secure_dns_policy_,
                  other.disable_cert_network_fetches_);
}

std::string HttpStreamKey::ToString() const {
  return base::StrCat(
      {disable_cert_network_fetches_ ? "disable_cert_network_fetches/" : "",
       ClientSocketPool::GroupId::GetSecureDnsPolicyGroupIdPrefix(
           secure_dns_policy_),
       ClientSocketPool::GroupId::GetPrivacyModeGroupIdPrefix(privacy_mode_),
       destination_.Serialize(),
       NetworkAnonymizationKey::IsPartitioningEnabled()
           ? base::StrCat(
                 {" <", network_anonymization_key_.ToDebugString(), ">"})
           : ""});
}

base::Value::Dict HttpStreamKey::ToValue() const {
  base::Value::Dict dict;
  dict.Set("destination", destination_.Serialize());
  dict.Set("privacy_mode", PrivacyModeToDebugString(privacy_mode_));
  dict.Set("network_anonymization_key",
           network_anonymization_key_.ToDebugString());
  dict.Set("secure_dns_policy",
           SecureDnsPolicyToDebugString(secure_dns_policy_));
  dict.Set("disable_cert_network_fetches", disable_cert_network_fetches_);
  return dict;
}

SpdySessionKey HttpStreamKey::ToSpdySessionKey() const {
  HostPortPair host_port = GURL::SchemeIsCryptographic(destination().scheme())
                               ? HostPortPair::FromSchemeHostPort(destination())
                               : HostPortPair();
  return SpdySessionKey(std::move(host_port), privacy_mode(),
                        ProxyChain::Direct(), SessionUsage::kDestination,
                        socket_tag(), network_anonymization_key(),
                        secure_dns_policy(), disable_cert_network_fetches());
}

QuicSessionKey HttpStreamKey::ToQuicSessionKey() const {
  HostPortPair host_port = GURL::SchemeIsCryptographic(destination().scheme())
                               ? HostPortPair::FromSchemeHostPort(destination())
                               : HostPortPair();
  return QuicSessionKey(std::move(host_port), privacy_mode(),
                        ProxyChain::Direct(), SessionUsage::kDestination,
                        socket_tag(), network_anonymization_key(),
                        secure_dns_policy(), /*require_dns_https_alpn=*/false);
}

}  // namespace net
