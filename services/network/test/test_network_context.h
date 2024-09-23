// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/net_buildflags.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/origin.h"

namespace net {
class NetworkAnonymizationKey;
class NetworkIsolationKey;
class IsolationInfo;
}  // namespace net

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
      const net::IsolationInfo& isolation_info,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      mojo::PendingRemote<mojom::CookieAccessObserver> observer) override {}
  void GetTrustTokenQueryAnswerer(
      mojo::PendingReceiver<mojom::TrustTokenQueryAnswerer> receiver,
      const url::Origin& top_frame_origin) override {}
  void GetStoredTrustTokenCounts(
      GetStoredTrustTokenCountsCallback callback) override {}
  void GetPrivateStateTokenRedemptionRecords(
      GetPrivateStateTokenRedemptionRecordsCallback callback) override {}
  void DeleteStoredTrustTokens(
      const url::Origin& issuer,
      DeleteStoredTrustTokensCallback callback) override {}
  void SetBlockTrustTokens(bool block) override {}
#if BUILDFLAG(ENABLE_REPORTING)
  void AddReportingApiObserver(
      mojo::PendingRemote<network::mojom::ReportingApiObserver> observer)
      override {}
#endif  // BUILDFLAG(ENABLE_REPORTING)
  void ClearNetworkingHistoryBetween(
      base::Time start_time,
      base::Time end_time,
      ClearNetworkingHistoryBetweenCallback callback) override {}
  void ClearHttpCache(base::Time start_time,
                      base::Time end_time,
                      mojom::ClearDataFilterPtr filter,
                      ClearHttpCacheCallback callback) override {}
  void ComputeHttpCacheSize(base::Time start_time,
                            base::Time end_time,
                            ComputeHttpCacheSizeCallback callback) override {}
  void ClearCorsPreflightCache(
      mojom::ClearDataFilterPtr filter,
      ClearCorsPreflightCacheCallback callback) override {}
  void ClearHostCache(mojom::ClearDataFilterPtr filter,
                      ClearHostCacheCallback callback) override {}
  void ClearHttpAuthCache(base::Time start_time,
                          base::Time end_time,
                          mojom::ClearDataFilterPtr filter,
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
  void ClearTrustTokenData(mojom::ClearDataFilterPtr filter,
                           ClearTrustTokenDataCallback callback) override {}
  void ClearTrustTokenSessionOnlyData(
      ClearTrustTokenSessionOnlyDataCallback callback) override {}
  void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) override {}
  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override {}
  void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) override {}
  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::Value::Dict body) override {}
  void QueueEnterpriseReport(const std::string& type,
                             const std::string& group,
                             const GURL& url,
                             base::Value::Dict body) override {}
  void QueueSignedExchangeReport(
      mojom::SignedExchangeReportPtr report,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {}
  void CloseAllConnections(CloseAllConnectionsCallback callback) override {}
  void CloseIdleConnections(CloseIdleConnectionsCallback callback) override {}
  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            mojom::NetworkConditionsPtr conditions) override {}
  void SetAcceptLanguage(const std::string& new_accept_language) override {}
  void SetEnableReferrers(bool enable_referrers) override {}
#if BUILDFLAG(IS_CT_SUPPORTED)
  void SetCTPolicy(mojom::CTPolicyPtr ct_policy) override {}
  void SetSCTAuditingMode(mojom::SCTAuditingMode mode) override {}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  void CreateUDPSocket(
      mojo::PendingReceiver<mojom::UDPSocket> receiver,
      mojo::PendingRemote<mojom::UDPSocketListener> listener) override {}
  void CreateRestrictedUDPSocket(
      const net::IPEndPoint& addr,
      mojom::RestrictedUDPSocketMode mode,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::RestrictedUDPSocketParamsPtr params,
      mojo::PendingReceiver<mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<mojom::UDPSocketListener> listener,
      mojom::NetworkContext::CreateRestrictedUDPSocketCallback callback)
      override {}
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      mojom::TCPServerSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPServerSocket> socket,
      CreateTCPServerSocketCallback callback) override {}
  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
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
      const net::SiteForCookies& site_for_cookies,
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      std::vector<mojom::HttpHeaderPtr> additional_headers,
      int32_t process_id,
      const url::Origin& origin,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
      const std::optional<base::UnguessableToken>& throttling_profile_id)
      override {}
  void CreateWebTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      std::vector<mojom::WebTransportCertificateFingerprintPtr> fingerprints,
      mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client)
      override {}
  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<::network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {}
  void CreateNetLogExporter(
      mojo::PendingReceiver<mojom::NetLogExporter> receiver) override {}
  void ResolveHost(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<mojom::ResolveHostClient> response_client) override {}
  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<mojom::HostResolver> receiver) override {}
  void NotifyExternalCacheHit(const GURL& url,
                              const std::string& http_method,
                              const net::NetworkIsolationKey& key,
                              bool include_credentials) override {}
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
  void VerifyCertificateForTesting(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& hostname,
      const std::string& ocsp_response,
      const std::string& sct_list,
      VerifyCertificateForTestingCallback callback) override {}
  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      mojom::CredentialsMode credentials_mode,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {}
#if BUILDFLAG(IS_P2P_ENABLED)
  void CreateP2PSocketManager(
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client,
      mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
          trusted_socket_manager,
      mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver)
      override {}
#endif  // BUILDFLAG(IS_P2P_ENABLED)
  void CreateMdnsResponder(
      mojo::PendingReceiver<mojom::MdnsResponder> responder_receiver) override {
  }
  void ResetURLLoaderFactories() override {}
  void GetViaObliviousHttp(
      mojom::ObliviousHttpRequestPtr request,
      mojo::PendingRemote<mojom::ObliviousHttpClient>) override {}
  void ForceReloadProxyConfig(
      ForceReloadProxyConfigCallback callback) override {}
  void ClearBadProxiesCache(ClearBadProxiesCacheCallback callback) override {}
  void DeleteDynamicDataForHost(
      const std::string& host,
      DeleteDynamicDataForHostCallback callback) override {}
  void AddDomainReliabilityContextForTesting(
      const url::Origin& origin,
      const GURL& upload_url,
      AddDomainReliabilityContextForTestingCallback callback) override {}
  void ForceDomainReliabilityUploadsForTesting(
      ForceDomainReliabilityUploadsForTestingCallback callback) override {}
  void SetSplitAuthCacheByNetworkAnonymizationKey(
      bool split_auth_cache_by_network_anonymization_key) override {}
  void SaveHttpAuthCacheProxyEntries(
      SaveHttpAuthCacheProxyEntriesCallback callback) override {}
  void LoadHttpAuthCacheProxyEntries(
      const base::UnguessableToken& cache_key,
      LoadHttpAuthCacheProxyEntriesCallback callback) override {}
  void AddAuthCacheEntry(
      const net::AuthChallengeInfo& challenge,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const net::AuthCredentials& credentials,
      AddAuthCacheEntryCallback callback) override {}
  void SetCorsNonWildcardRequestHeadersSupport(bool value) override {}
  void LookupServerBasicAuthCredentials(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      LookupServerBasicAuthCredentialsCallback callback) override {}
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LookupProxyAuthCredentials(
      const net::ProxyServer& proxy_server,
      const std::string& auth_scheme,
      const std::string& realm,
      LookupProxyAuthCredentialsCallback callback) override {}
#endif
  void SetSharedDictionaryCacheMaxSize(uint64_t cache_max_size) override {}
  void ClearSharedDictionaryCache(
      base::Time start_time,
      base::Time end_time,
      mojom::ClearDataFilterPtr filter,
      ClearSharedDictionaryCacheCallback callback) override {}
  void ClearSharedDictionaryCacheForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      ClearSharedDictionaryCacheForIsolationKeyCallback callback) override {}
  void GetSharedDictionaryUsageInfo(
      GetSharedDictionaryUsageInfoCallback callback) override {}
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      GetSharedDictionaryInfoCallback callback) override {}
  void GetSharedDictionaryOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      GetSharedDictionaryOriginsBetweenCallback callback) override {}
  void PreloadSharedDictionaryInfoForDocument(
      const std::vector<GURL>& urls,
      mojo::PendingReceiver<mojom::PreloadedSharedDictionaryInfoHandle>
          preload_handle) override {}
  void HasPreloadedSharedDictionaryInfoForTesting(
      HasPreloadedSharedDictionaryInfoForTestingCallback callback) override {}
  void ResourceSchedulerClientVisibilityChanged(
      const base::UnguessableToken& client_token,
      bool visible) override {}
  void FlushCachedClientCertIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate) override {}
  void FlushMatchingCachedClientCert(
      const scoped_refptr<net::X509Certificate>& certificate) override {}
  void SetCookieDeprecationLabel(
      const std::optional<std::string>& label) override {}
  void RevokeNetworkForNonces(
      const std::vector<base::UnguessableToken>& nonces,
      RevokeNetworkForNoncesCallback callback) override {}
  void ClearNonces(const std::vector<base::UnguessableToken>& nonces) override {
  }
  void ExemptUrlFromNetworkRevocationForNonce(
      const GURL& exempted_url,
      const base::UnguessableToken& nonce,
      ExemptUrlFromNetworkRevocationForNonceCallback callback) override {}
  void Prefetch(int32_t request_id,
                uint32_t options,
                const ResourceRequest& request,
                const net::MutableNetworkTrafficAnnotationTag&
                    traffic_annotation) override {}
  void GetBoundNetworkForTesting(
      GetBoundNetworkForTestingCallback callback) override {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_H_
