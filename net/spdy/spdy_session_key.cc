// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_key.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/dns/public/secure_dns_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

SpdySessionKey::SpdySessionKey() = default;

SpdySessionKey::SpdySessionKey(
    const HostPortPair& host_port_pair,
    const ProxyChain& proxy_chain,
    PrivacyMode privacy_mode,
    IsProxySession is_proxy_session,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy)
    : host_port_proxy_pair_(host_port_pair, proxy_chain),
      privacy_mode_(privacy_mode),
      is_proxy_session_(is_proxy_session),
      socket_tag_(socket_tag),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? network_anonymization_key
              : NetworkAnonymizationKey()),
      secure_dns_policy_(secure_dns_policy) {
  DVLOG(1) << "SpdySessionKey(host=" << host_port_pair.ToString()
           << ", proxy_chain=" << proxy_chain << ", privacy=" << privacy_mode;
}

SpdySessionKey::SpdySessionKey(const SpdySessionKey& other) = default;

SpdySessionKey::~SpdySessionKey() = default;

bool SpdySessionKey::operator<(const SpdySessionKey& other) const {
  return std::tie(privacy_mode_, host_port_proxy_pair_.first,
                  host_port_proxy_pair_.second, is_proxy_session_,
                  network_anonymization_key_, secure_dns_policy_, socket_tag_) <
         std::tie(other.privacy_mode_, other.host_port_proxy_pair_.first,
                  other.host_port_proxy_pair_.second, other.is_proxy_session_,
                  other.network_anonymization_key_, other.secure_dns_policy_,
                  other.socket_tag_);
}

bool SpdySessionKey::operator==(const SpdySessionKey& other) const {
  return privacy_mode_ == other.privacy_mode_ &&
         host_port_proxy_pair_.first.Equals(
             other.host_port_proxy_pair_.first) &&
         host_port_proxy_pair_.second == other.host_port_proxy_pair_.second &&
         is_proxy_session_ == other.is_proxy_session_ &&
         network_anonymization_key_ == other.network_anonymization_key_ &&
         secure_dns_policy_ == other.secure_dns_policy_ &&
         socket_tag_ == other.socket_tag_;
}

bool SpdySessionKey::operator!=(const SpdySessionKey& other) const {
  return !(*this == other);
}

SpdySessionKey::CompareForAliasingResult SpdySessionKey::CompareForAliasing(
    const SpdySessionKey& other) const {
  CompareForAliasingResult result;
  result.is_potentially_aliasable =
      (privacy_mode_ == other.privacy_mode_ &&
       host_port_proxy_pair_.second == other.host_port_proxy_pair_.second &&
       is_proxy_session_ == other.is_proxy_session_ &&
       network_anonymization_key_ == other.network_anonymization_key_ &&
       secure_dns_policy_ == other.secure_dns_policy_);
  result.is_socket_tag_match = (socket_tag_ == other.socket_tag_);
  return result;
}

}  // namespace net
