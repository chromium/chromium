// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_key.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"

namespace net {

SpdySessionKey::SpdySessionKey() = default;

SpdySessionKey::SpdySessionKey(const HostPortPair& host_port_pair,
                               const ProxyServer& proxy_server,
                               PrivacyMode privacy_mode,
                               IsProxySession is_proxy_session,
                               const SocketTag& socket_tag,
                               const NetworkIsolationKey& network_isolation_key,
                               bool disable_secure_dns)
    : host_port_proxy_pair_(host_port_pair, proxy_server),
      privacy_mode_(privacy_mode),
      is_proxy_session_(is_proxy_session),
      socket_tag_(socket_tag),
      network_isolation_key_(
          base::FeatureList::IsEnabled(
              features::kPartitionConnectionsByNetworkIsolationKey)
              ? network_isolation_key
              : NetworkIsolationKey()),
      disable_secure_dns_(disable_secure_dns) {
  // IsProxySession::kTrue should only be used with direct connections, since
  // using multiple layers of proxies on top of each other isn't supported.
  DCHECK(is_proxy_session != IsProxySession::kTrue || proxy_server.is_direct());
  DVLOG(1) << "SpdySessionKey(host=" << host_port_pair.ToString()
      << ", proxy=" << proxy_server.ToURI()
      << ", privacy=" << privacy_mode;
}

SpdySessionKey::SpdySessionKey(const SpdySessionKey& other) = default;

SpdySessionKey::~SpdySessionKey() = default;

bool SpdySessionKey::operator<(const SpdySessionKey& other) const {
  return std::tie(privacy_mode_, host_port_proxy_pair_.first,
                  host_port_proxy_pair_.second, is_proxy_session_,
                  network_isolation_key_, disable_secure_dns_, socket_tag_) <
         std::tie(other.privacy_mode_, other.host_port_proxy_pair_.first,
                  other.host_port_proxy_pair_.second, other.is_proxy_session_,
                  other.network_isolation_key_, other.disable_secure_dns_,
                  other.socket_tag_);
}

bool SpdySessionKey::operator==(const SpdySessionKey& other) const {
  return privacy_mode_ == other.privacy_mode_ &&
         host_port_proxy_pair_.first.Equals(
             other.host_port_proxy_pair_.first) &&
         host_port_proxy_pair_.second == other.host_port_proxy_pair_.second &&
         is_proxy_session_ == other.is_proxy_session_ &&
         network_isolation_key_ == other.network_isolation_key_ &&
         disable_secure_dns_ == other.disable_secure_dns_ &&
         socket_tag_ == other.socket_tag_;
}

bool SpdySessionKey::operator!=(const SpdySessionKey& other) const {
  return !(*this == other);
}

size_t SpdySessionKey::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(host_port_proxy_pair_);
}

}  // namespace net
