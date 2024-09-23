// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_FACTORY_H_
#define NET_SOCKET_CONNECT_JOB_FACTORY_H_

#include <memory>
#include <optional>
#include <vector>

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
#include "net/ssl/ssl_config.h"
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
  // What protocols may be negotiated with the destination SSL server via ALPN.
  // These do not apply to the proxy server, for which all protocols listed in
  // CommonConnectJobParams are always allowed to be negotiated, unless
  // HttpServerProperties forces H1.
  //
  // AlpnMode has no impact when not talking to an HTTPS destination server.
  enum class AlpnMode {
    // Don't use ALPN mode at all when negotiating a connection. This is used by
    // non-HTTP consumers.
    kDisabled,
    // Only try to negotiate H1. This is only used by WebSockets.
    kHttp11Only,
    // Allow negotiating H2 or H1 via ALPN. H2 may only be negotiated if
    // CommonConnectJobParams allows it. Also, if HttpServerProperties only
    // allows H1 for the destination server, only H1 will be negotiated, even
    // if `kHttpAll` is specified.
    kHttpAll,
  };

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
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      ConnectJobFactory::AlpnMode alpn_mode,
      bool force_tunnel,
      PrivacyMode privacy_mode,
      const OnHostResolutionCallback& resolution_callback,
      RequestPriority request_priority,
      SocketTag socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      bool disable_cert_network_fetches,
      const CommonConnectJobParams* common_connect_job_params,
      ConnectJob::Delegate* delegate) const;

  // TODO(crbug.com/40181080): Rename to discourage use except in cases where
  // the scheme is non-standard or unknown.
  std::unique_ptr<ConnectJob> CreateConnectJob(
      bool using_ssl,
      HostPortPair endpoint,
      const ProxyChain& proxy_chain,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
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
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      ConnectJobFactory::AlpnMode alpn_mode,
      bool force_tunnel,
      PrivacyMode privacy_mode,
      const OnHostResolutionCallback& resolution_callback,
      RequestPriority request_priority,
      SocketTag socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      bool disable_cert_network_fetches,
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
