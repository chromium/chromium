// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_REQUEST_INFO_H_
#define NET_HTTP_HTTP_STREAM_POOL_REQUEST_INFO_H_

#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/http/alternative_service.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "url/scheme_host_port.h"

namespace net {

// Contains information to request a stream/preconnect from the HttpStreamPool.
struct NET_EXPORT_PRIVATE HttpStreamPoolRequestInfo {
  HttpStreamPoolRequestInfo(url::SchemeHostPort destination,
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
                            NetLogWithSource factory_job_controller_net_log);

  HttpStreamPoolRequestInfo(HttpStreamPoolRequestInfo&&);
  HttpStreamPoolRequestInfo& operator=(HttpStreamPoolRequestInfo&&);

  // Move-only.
  HttpStreamPoolRequestInfo(const HttpStreamPoolRequestInfo&) = delete;
  HttpStreamPoolRequestInfo& operator=(const HttpStreamPoolRequestInfo&) =
      delete;

  ~HttpStreamPoolRequestInfo();

  url::SchemeHostPort destination;
  PrivacyMode privacy_mode = PrivacyMode::PRIVACY_MODE_DISABLED;
  SocketTag socket_tag;
  NetworkAnonymizationKey network_anonymization_key;
  SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;
  bool disable_cert_network_fetches = false;

  AlternativeServiceInfo alternative_service_info;
  AdvertisedAltSvcState advertised_alt_svc_state =
      AdvertisedAltSvcState::kUnknown;

  NextProtoSet allowed_alpns;
  int load_flags = 0;
  ProxyInfo proxy_info;

  NetLogWithSource factory_job_controller_net_log;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_REQUEST_INFO_H_
