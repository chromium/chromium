// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_request_info.h"

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/alternative_service.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "url/scheme_host_port.h"

namespace net {

HttpStreamPoolRequestInfo::HttpStreamPoolRequestInfo(
    url::SchemeHostPort destination,
    PrivacyMode privacy_mode,
    SocketTag socket_tag,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool disable_cert_network_fetches,
    AlternativeServiceInfo alternative_service_info,
    AdvertisedAltSvcState advertised_alt_svc_state,
    NextProtoSet allowed_alpns,
    int load_flags,
    ProxyInfo proxy_info,
    NetLogWithSource factory_job_controller_net_log)
    : destination(std::move(destination)),
      privacy_mode(privacy_mode),
      socket_tag(std::move(socket_tag)),
      network_anonymization_key(NetworkAnonymizationKey::IsPartitioningEnabled()
                                    ? std::move(network_anonymization_key)
                                    : NetworkAnonymizationKey()),
      secure_dns_policy(secure_dns_policy),
      disable_cert_network_fetches(disable_cert_network_fetches),
      alternative_service_info(std::move(alternative_service_info)),
      advertised_alt_svc_state(advertised_alt_svc_state),
      allowed_alpns(allowed_alpns),
      load_flags(load_flags),
      proxy_info(std::move(proxy_info)),
      factory_job_controller_net_log(
          std::move(factory_job_controller_net_log)) {}

HttpStreamPoolRequestInfo::HttpStreamPoolRequestInfo(
    HttpStreamPoolRequestInfo&&) = default;

HttpStreamPoolRequestInfo& HttpStreamPoolRequestInfo::operator=(
    HttpStreamPoolRequestInfo&&) = default;

HttpStreamPoolRequestInfo::~HttpStreamPoolRequestInfo() = default;

}  // namespace net
