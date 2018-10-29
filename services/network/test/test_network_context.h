// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/origin.h"

namespace network {

// Noop implementation of mojom::NetworkContext.  Useful to override to create
// specialized mocks or fakes.
class TestNetworkContext : public mojom::NetworkContext {
 public:
  TestNetworkContext() = default;
  ~TestNetworkContext() override = default;

  void SetClient(mojom::NetworkContextClientPtr client) override {}
  void CreateURLLoaderFactory(
      mojom::URLLoaderFactoryRequest request,
      mojom::URLLoaderFactoryParamsPtr params) override {}
  void GetCookieManager(mojom::CookieManagerRequest cookie_manager) override {}
  void GetRestrictedCookieManager(
      mojom::RestrictedCookieManagerRequest restricted_cookie_manager,
      const url::Origin& origin) override {}
  void ClearNetworkingHistorySince(
      base::Time start_time,
      ClearNetworkingHistorySinceCallback callback) override {}
  void ClearHttpCache(base::Time start_time,
                      base::Time end_time,
                      mojom::ClearDataFilterPtr filter,
                      ClearHttpCacheCallback callback) override {}
  void ComputeHttpCacheSize(base::Time start_time,
                            base::Time end_time,
                            ComputeHttpCacheSizeCallback callback) override {}
  void ClearChannelIds(base::Time start_time,
                       base::Time end_time,
                       mojom::ClearDataFilterPtr filter,
                       ClearChannelIdsCallback callback) override {}
  void ClearHostCache(mojom::ClearDataFilterPtr filter,
                      ClearHostCacheCallback callback) override {}
  void ClearHttpAuthCache(base::Time start_time,
                          ClearHttpAuthCacheCallback callback) override {}
  void ClearReportingCacheReports(
      mojom::ClearDataFilterPtr filter,
      ClearReportingCacheReportsCallback callback) override {}
  void ClearReportingCacheClients(
      mojom::ClearDataFilterPtr filter,
      ClearReportingCacheClientsCallback callback) override {}
  void ClearNetworkErrorLogging(
      mojom::ClearDataFilterPtr filter,
      ClearNetworkErrorLoggingCallback callback) override {}
  void CloseAllConnections(CloseAllConnectionsCallback callback) override {}
  void CloseIdleConnections(CloseIdleConnectionsCallback callback) override {}
  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            mojom::NetworkConditionsPtr conditions) override {}
  void SetAcceptLanguage(const std::string& new_accept_language) override {}
  void SetEnableReferrers(bool enable_referrers) override {}
  void SetCTPolicy(
      const std::vector<std::string>& required_hosts,
      const std::vector<std::string>& excluded_hosts,
      const std::vector<std::string>& excluded_spkis,
      const std::vector<std::string>& excluded_legacy_spkis) override {}
#if defined(OS_CHROMEOS)
  void UpdateTrustAnchors(const net::CertificateList& trust_anchors) override {}
#endif
  void AddExpectCT(const std::string& domain,
                   base::Time expiry,
                   bool enforce,
                   const GURL& report_uri,
                   AddExpectCTCallback callback) override {}
  void SetExpectCTTestReport(const GURL& report_uri,
                             SetExpectCTTestReportCallback callback) override {}
  void GetExpectCTState(const std::string& domain,
                        GetExpectCTStateCallback callback) override {}
  void CreateUDPSocket(mojom::UDPSocketRequest request,
                       mojom::UDPSocketReceiverPtr receiver) override {}
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPServerSocketRequest socket,
      CreateTCPServerSocketCallback callback) override {}
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPConnectedSocketRequest socket,
      mojom::SocketObserverPtr observer,
      CreateTCPConnectedSocketCallback callback) override {}
  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPBoundSocketRequest request,
      CreateTCPBoundSocketCallback callback) override {}
  void CreateProxyResolvingSocketFactory(
      mojom::ProxyResolvingSocketFactoryRequest request) override {}
  void CreateWebSocket(mojom::WebSocketRequest request,
                       int32_t process_id,
                       int32_t render_frame_id,
                       const url::Origin& origin,
                       mojom::AuthenticationHandlerPtr auth_handler) override {}
  void LookUpProxyForURL(
      const GURL& url,
      ::network::mojom::ProxyLookupClientPtr proxy_lookup_client) override {}
  void CreateNetLogExporter(mojom::NetLogExporterRequest exporter) override {}
  void ResolveHost(const net::HostPortPair& host,
                   mojom::ResolveHostParametersPtr optional_parameters,
                   mojom::ResolveHostClientPtr response_client) override {}
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojom::HostResolverRequest request) override {}
  void WriteCacheMetadata(const GURL& url,
                          net::RequestPriority priority,
                          base::Time expected_response_time,
                          const std::vector<uint8_t>& data) override {}
  void VerifyCertForSignedExchange(
      const scoped_refptr<net::X509Certificate>& certificate,
      const GURL& url,
      const std::string& ocsp_result,
      const std::string& sct_list,
      VerifyCertForSignedExchangeCallback callback) override {}
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override {}
  void SetCorsOriginAccessListsForOrigin(
      const url::Origin& source_origin,
      std::vector<mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override {}
  void AddHSTS(const std::string& host,
               base::Time expiry,
               bool include_subdomains,
               AddHSTSCallback callback) override {}
  void GetHSTSState(const std::string& domain,
                    GetHSTSStateCallback callback) override {}
  void EnableStaticKeyPinningForTesting(
      EnableStaticKeyPinningForTestingCallback callback) override {}
  void SetFailingHttpTransactionForTesting(
      int32_t rv,
      SetFailingHttpTransactionForTestingCallback callback) override {}
  void VerifyCertificateForTesting(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& hostname,
      const std::string& ocsp_response,
      VerifyCertificateForTestingCallback callback) override {}
  void PreconnectSockets(uint32_t num_streams,
                         const GURL& url,
                         int32_t load_flags,
                         bool privacy_mode_enabled) override {}
  void CreateP2PSocketManager(
      mojom::P2PTrustedSocketManagerClientPtr client,
      mojom::P2PTrustedSocketManagerRequest trusted_socket_manager,
      mojom::P2PSocketManagerRequest socket_manager_request) override {}
  void ResetURLLoaderFactories() override {}
  void ForceReloadProxyConfig(
      ForceReloadProxyConfigCallback callback) override {}
  void ClearBadProxiesCache(ClearBadProxiesCacheCallback callback) override {}
  void DeleteDynamicDataForHost(
      const std::string& host,
      DeleteDynamicDataForHostCallback callback) override {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_
