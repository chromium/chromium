// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_FACTORY_H_
#define NET_SOCKET_CONNECT_JOB_FACTORY_H_

#include <memory>

#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

class NetworkAnonymizationKey;
struct NetworkTrafficAnnotationTag;
class ProxyChain;
struct SSLConfig;

// Common factory for all ConnectJob types. Determines and creates the correct
// ConnectJob depending on the passed in parameters.
class NET_EXPORT_PRIVATE ConnectJobFactory {
 public:
  // The endpoint of a connection when the endpoint does not have a known
  // standard scheme.
  struct SchemelessEndpoint {
    bool using_ssl;
    HostPortPair host_port_pair;
  };

  // Representation of the endpoint of a connection. Could be schemeful or
  // schemeless.
  using Endpoint = absl::variant<url::SchemeHostPort, SchemelessEndpoint>;

  // Default factory will be used if passed the default `nullptr`.
  explicit ConnectJobFactory(
      std::unique_ptr<HttpProxyConnectJob::Factory>
          http_proxy_connect_job_factory = nullptr,
      std::unique_ptr<SOCKSConnectJob::Factory> socks_connect_job_factory =
          nullptr,
      std::unique_ptr<SSLConnectJob::Factory> ssl_connect_job_factory = nullptr,
      std::unique_ptr<TransportConnectJob::Factory>
          transport_connect_job_factory = nullptr);

  // Not copyable/movable. Intended for polymorphic use via pointer.
  ConnectJobFactory(const ConnectJobFactory&) = delete;
  ConnectJobFactory& operator=(const ConnectJobFactory&) = delete;

  virtual ~ConnectJobFactory();

  // `common_connect_job_params` and `delegate` must outlive the returned
  // ConnectJob.
  std::unique_ptr<ConnectJob> CreateConnectJob(
      url::SchemeHostPort endpoint,
      const ProxyChain& proxy_chain,
      const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      const SSLConfig* ssl_config_for_origin,
      const SSLConfig* base_ssl_config_for_proxies,
      bool force_tunnel,
      PrivacyMode privacy_mode,
      const OnHostResolutionCallback& resolution_callback,
      RequestPriority request_priority,
      SocketTag socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      const CommonConnectJobParams* common_connect_job_params,
      ConnectJob::Delegate* delegate) const;

  // TODO(crbug.com/1206799): Rename to discourage use except in cases where the
  // scheme is non-standard or unknown.
  std::unique_ptr<ConnectJob> CreateConnectJob(
      bool using_ssl,
      HostPortPair endpoint,
      const ProxyChain& proxy_chain,
      const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      const SSLConfig* ssl_config_for_origin,
      const SSLConfig* base_ssl_config_for_proxies,
      bool force_tunnel,
      PrivacyMode privacy_mode,
      const OnHostResolutionCallback& resolution_callback,
      RequestPriority request_priority,
      SocketTag socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      const CommonConnectJobParams* common_connect_job_params,
      ConnectJob::Delegate* delegate) const;

 private:
  virtual std::unique_ptr<ConnectJob> CreateConnectJob(
      Endpoint endpoint,
      const ProxyChain& proxy_chain,
      const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      const SSLConfig* ssl_config_for_origin,
      const SSLConfig* base_ssl_config_for_proxies,
      bool force_tunnel,
      PrivacyMode privacy_mode,
      const OnHostResolutionCallback& resolution_callback,
      RequestPriority request_priority,
      SocketTag socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      const CommonConnectJobParams* common_connect_job_params,
      ConnectJob::Delegate* delegate) const;

  std::unique_ptr<HttpProxyConnectJob::Factory> http_proxy_connect_job_factory_;
  std::unique_ptr<SOCKSConnectJob::Factory> socks_connect_job_factory_;
  std::unique_ptr<SSLConnectJob::Factory> ssl_connect_job_factory_;
  std::unique_ptr<TransportConnectJob::Factory> transport_connect_job_factory_;

  // Use a single NetworkAnonymizationKey for looking up proxy hostnames.
  // Proxies are typically used across sites, but cached proxy IP addresses
  // don't really expose useful information to destination sites, and not
  // caching them has a performance cost.
  net::NetworkAnonymizationKey proxy_dns_network_anonymization_key_ =
      net::NetworkAnonymizationKey::CreateTransient();
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_FACTORY_H_
