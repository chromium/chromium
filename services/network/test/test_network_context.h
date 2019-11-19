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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_service_buildflags.h"
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

namespace net {
class NetworkIsolationKey;
}

namespace network {

// Noop implementation of mojom::NetworkContext.  Useful to override to create
// specialized mocks or fakes.
class TestNetworkContext : public mojom::NetworkContext {
 public:
  TestNetworkContext() = default;
  ~TestNetworkContext() override = default;

  void SetClient(
      mojo::PendingRemote<mojom::NetworkContextClient> client) override {}
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      mojom::URLLoaderFactoryParamsPtr params) override {}
  void GetCookieManager(
      mojo::PendingReceiver<mojom::CookieManager> cookie_manager) override {}
  void GetRestrictedCookieManager(
      mojo::PendingReceiver<mojom::RestrictedCookieManager>
          restricted_cookie_manager,
      mojom::RestrictedCookieManagerRole role,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id) override {}
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
  void ClearDomainReliability(
      mojom::ClearDataFilterPtr filter,
      DomainReliabilityClearMode mode,
      ClearDomainReliabilityCallback callback) override {}
  void GetDomainReliabilityJSON(
      GetDomainReliabilityJSONCallback callback) override {}
  void QueueReport(const std::string& type,
                   const std::string& group,
                   const GURL& url,
                   const base::Optional<std::string>& user_agent,
                   base::Value body) override {}
  void QueueSignedExchangeReport(
      mojom::SignedExchangeReportPtr report) override {}
  void CloseAllConnections(CloseAllConnectionsCallback callback) override {}
  void CloseIdleConnections(CloseIdleConnectionsCallback callback) override {}
  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            mojom::NetworkConditionsPtr conditions) override {}
  void SetAcceptLanguage(const std::string& new_accept_language) override {}
  void SetEnableReferrers(bool enable_referrers) override {}
#if defined(OS_CHROMEOS)
  void UpdateAdditionalCertificates(
      mojom::AdditionalCertificatesPtr additional_certificates) override {}
#endif
#if BUILDFLAG(IS_CT_SUPPORTED)
  void SetCTPolicy(
      const std::vector<std::string>& required_hosts,
      const std::vector<std::string>& excluded_hosts,
      const std::vector<std::string>& excluded_spkis,
      const std::vector<std::string>& excluded_legacy_spkis) override {}
  void AddExpectCT(const std::string& domain,
                   base::Time expiry,
                   bool enforce,
                   const GURL& report_uri,
                   AddExpectCTCallback callback) override {}
  void SetExpectCTTestReport(const GURL& report_uri,
                             SetExpectCTTestReportCallback callback) override {}
  void GetExpectCTState(const std::string& domain,
                        GetExpectCTStateCallback callback) override {}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  void CreateUDPSocket(
      mojo::PendingReceiver<mojom::UDPSocket> receiver,
      mojo::PendingRemote<mojom::UDPSocketListener> listener) override {}
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPServerSocket> socket,
      CreateTCPServerSocketCallback callback) override {}
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {}
  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPBoundSocket> receiver,
      CreateTCPBoundSocketCallback callback) override {}
  void CreateProxyResolvingSocketFactory(
      mojo::PendingReceiver<mojom::ProxyResolvingSocketFactory> receiver)
      override {}
  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      const GURL& site_for_cookies,
      const net::NetworkIsolationKey& network_isolation_key,
      std::vector<mojom::HttpHeaderPtr> additional_headers,
      int32_t process_id,
      int32_t render_frame_id,
      const url::Origin& origin,
      uint32_t options,
      mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::AuthenticationHandler> auth_handler,
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client) override {}
  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client)
      override {}
  void LookUpProxyForURL(
      const GURL& url,
      mojo::PendingRemote<::network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {}
  void CreateNetLogExporter(
      mojo::PendingReceiver<mojom::NetLogExporter> receiver) override {}
  void ResolveHost(
      const net::HostPortPair& host,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<mojom::ResolveHostClient> response_client) override {}
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<mojom::HostResolver> receiver) override {}
  void NotifyExternalCacheHit(const GURL& url,
                              const std::string& http_method,
                              const net::NetworkIsolationKey& key) override {}
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
  void SetCorsExtraSafelistedRequestHeaderNames(
      const std::vector<std::string>&
          cors_extra_safelisted_request_header_names) override {}
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
      const std::string& sct_list,
      VerifyCertificateForTestingCallback callback) override {}
  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      bool allow_credentials,
      const net::NetworkIsolationKey& network_isolation_key) override {}
  void CreateP2PSocketManager(
      mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client,
      mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
          trusted_socket_manager,
      mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver)
      override {}
  void CreateMdnsResponder(
      mojo::PendingReceiver<mojom::MdnsResponder> responder_receiver) override {
  }
  void ResetURLLoaderFactories() override {}
  void ForceReloadProxyConfig(
      ForceReloadProxyConfigCallback callback) override {}
  void ClearBadProxiesCache(ClearBadProxiesCacheCallback callback) override {}
  void DeleteDynamicDataForHost(
      const std::string& host,
      DeleteDynamicDataForHostCallback callback) override {}
  void AddDomainReliabilityContextForTesting(
      const GURL& origin,
      const GURL& upload_url,
      AddDomainReliabilityContextForTestingCallback callback) override {}
  void ForceDomainReliabilityUploadsForTesting(
      ForceDomainReliabilityUploadsForTestingCallback callback) override {}
  void SetSplitAuthCacheByNetworkIsolationKey(
      bool split_auth_cache_by_network_isolation_key) override {}
  void SaveHttpAuthCacheProxyEntries(
      SaveHttpAuthCacheProxyEntriesCallback callback) override {}
  void LoadHttpAuthCacheProxyEntries(
      const base::UnguessableToken& cache_key,
      LoadHttpAuthCacheProxyEntriesCallback callback) override {}
  void AddAuthCacheEntry(const net::AuthChallengeInfo& challenge,
                         const net::NetworkIsolationKey& network_isolation_key,
                         const net::AuthCredentials& credentials,
                         AddAuthCacheEntryCallback callback) override {}
  void LookupServerBasicAuthCredentials(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      LookupServerBasicAuthCredentialsCallback callback) override {}
  void GetOriginPolicyManager(
      mojo::PendingReceiver<mojom::OriginPolicyManager> receiver) override {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_
