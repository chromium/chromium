// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_CONTEXT_H_
#define SERVICES_NETWORK_NETWORK_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/http/http_auth_preferences.h"
#include "net/net_buildflags.h"
#include "net/reporting/reporting_target_type.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/first_party_sets/first_party_sets_access_delegate.h"
#include "services/network/http_cache_data_counter.h"
#include "services/network/http_cache_data_remover.h"
#include "services/network/network_qualities_pref_delegate.h"
#include "services/network/oblivious_http_request_handler.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/transferable_directory.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/socket_factory.h"
#include "services/network/url_request_context_owner.h"
#include "services/network/web_bundle/web_bundle_manager.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_report.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/public/mojom/ct_log_info.mojom-forward.h"
#endif

namespace base {
class UnguessableToken;
}  // namespace base

namespace net {
class CertVerifier;
class HostPortPair;
class IsolationInfo;
class NetworkAnonymizationKey;
class StaticHttpUserAgentSettings;
class URLRequestContext;
class URLRequestContextBuilder;
}  // namespace net

namespace certificate_transparency {
class ChromeRequireCTDelegate;
}  // namespace certificate_transparency

namespace domain_reliability {
class DomainReliabilityMonitor;
}  // namespace domain_reliability

namespace url_matcher {
class URLMatcher;
}

namespace network {
class CookieManager;
class HostResolver;
class MdnsResponderManager;
class MojoBackendFileOperationsFactory;
class NetworkService;
class NetworkServiceNetworkDelegate;
class P2PSocketManager;
class PendingTrustTokenStore;
class PrefetchCache;
class PrefetchMatchingURLLoaderFactory;
class ProxyLookupRequest;
class ResourceSchedulerClient;
class SCTAuditingHandler;
class SQLiteTrustTokenPersister;
class SessionCleanupCookieStore;
class SharedDictionaryManager;
class WebSocketFactory;
class WebTransport;

struct ResourceRequest;

// A NetworkContext creates and manages access to a URLRequestContext.
//
// When the network service is enabled, NetworkContexts are created through
// NetworkService's mojo interface and are owned jointly by the NetworkService
// and the mojo::Remote<NetworkContext> used to talk to them, and the
// NetworkContext is destroyed when either one is torn down.
#if BUILDFLAG(ENABLE_REPORTING)
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkContext
    : public mojom::NetworkContext,
      public net::ReportingCacheObserver {
#else
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkContext
    : public mojom::NetworkContext {
#endif  // BUILDFLAG(ENABLE_REPORTING)

 public:
  using OnConnectionCloseCallback =
      base::OnceCallback<void(NetworkContext* network_context)>;
  using OnURLRequestContextBuilderConfiguredCallback =
      base::OnceCallback<void(net::URLRequestContextBuilder*)>;

  NetworkContext(NetworkService* network_service,
                 mojo::PendingReceiver<mojom::NetworkContext> receiver,
                 mojom::NetworkContextParamsPtr params,
                 OnConnectionCloseCallback on_connection_close_callback =
                     OnConnectionCloseCallback());

  // DEPRECATED: Creates a NetworkContext that simply wraps a consumer-provided
  // URLRequestContext that is not owned by the NetworkContext.
  // TODO(mmenke):  Remove this constructor when the network service ships.
  NetworkContext(NetworkService* network_service,
                 mojo::PendingReceiver<mojom::NetworkContext> receiver,
                 net::URLRequestContext* url_request_context,
                 const std::vector<std::string>& cors_exempt_header_list);

  NetworkContext(base::PassKey<NetworkContext> pass_key,
                 NetworkService* network_service,
                 mojo::PendingReceiver<mojom::NetworkContext> receiver,
                 mojom::NetworkContextParamsPtr params,
                 OnConnectionCloseCallback on_connection_close_callback,
                 OnURLRequestContextBuilderConfiguredCallback
                     on_url_request_context_builder_configured);

  NetworkContext(const NetworkContext&) = delete;
  NetworkContext& operator=(const NetworkContext&) = delete;

  ~NetworkContext() override;

  static std::unique_ptr<NetworkContext> CreateForTesting(
      NetworkService* network_service,
      mojo::PendingReceiver<mojom::NetworkContext> receiver,
      mojom::NetworkContextParamsPtr params,
      OnURLRequestContextBuilderConfiguredCallback
          on_url_request_context_builder_configured);
  // Sets a global CertVerifier to use when initializing all profiles.
  static void SetCertVerifierForTesting(net::CertVerifier* cert_verifier);

  net::URLRequestContext* url_request_context() { return url_request_context_; }

  NetworkService* network_service() { return network_service_; }

  mojom::NetworkContextClient* client() {
    return client_.is_bound() ? client_.get() : nullptr;
  }

  ResourceScheduler* resource_scheduler() { return resource_scheduler_.get(); }

  CookieManager* cookie_manager() { return cookie_manager_.get(); }

  const base::flat_set<std::string>* cors_exempt_header_list() const {
    return &cors_exempt_header_list_;
  }

  bool allow_any_cors_exempt_header_for_browser() const {
    return params_ && params_->allow_any_cors_exempt_header_for_browser;
  }

#if BUILDFLAG(IS_ANDROID)
  const std::vector<std::unique_ptr<base::android::ApplicationStatusListener>>&
  app_status_listeners() const {
    return app_status_listeners_;
  }
#endif

  // Creates a URLLoaderFactory with a ResourceSchedulerClient specified. This
  // is used to reuse the existing ResourceSchedulerClient for cloned
  // URLLoaderFactory.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client);

  // Creates a URLLoaderFactory with params specific to the
  // CertVerifierService. A URLLoaderFactory created by this function will be
  // used by a CertNetFetcherURLLoader to perform AIA and OCSP fetching.
  // These URLLoaderFactories should only ever be used by the
  // CertVerifierService, and should never be passed to a renderer.
  void CreateURLLoaderFactoryForCertNetFetcher(
      mojo::PendingReceiver<mojom::URLLoaderFactory> factory_receiver);

  // Enables DoH probes to be sent using this context whenever the DNS
  // configuration contains DoH servers.
  void ActivateDohProbes();

  // mojom::NetworkContext implementation:
  void SetClient(
      mojo::PendingRemote<mojom::NetworkContextClient> client) override;
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      mojom::URLLoaderFactoryParamsPtr params) override;
  void ResetURLLoaderFactories() override;
  void GetViaObliviousHttp(
      mojom::ObliviousHttpRequestPtr request,
      mojo::PendingRemote<mojom::ObliviousHttpClient> client) override;
  void GetCookieManager(
      mojo::PendingReceiver<mojom::CookieManager> receiver) override;
  void GetRestrictedCookieManager(
      mojo::PendingReceiver<mojom::RestrictedCookieManager> receiver,
      mojom::RestrictedCookieManagerRole role,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      mojo::PendingRemote<mojom::CookieAccessObserver> observer) override;
  void GetTrustTokenQueryAnswerer(
      mojo::PendingReceiver<mojom::TrustTokenQueryAnswerer> receiver,
      const url::Origin& top_frame_origin) override;
  void ClearTrustTokenData(mojom::ClearDataFilterPtr filter,
                           base::OnceClosure done) override;
  void ClearTrustTokenSessionOnlyData(
      ClearTrustTokenSessionOnlyDataCallback callback) override;
  void GetStoredTrustTokenCounts(
      GetStoredTrustTokenCountsCallback callback) override;
  void GetPrivateStateTokenRedemptionRecords(
      GetPrivateStateTokenRedemptionRecordsCallback callback) override;
  void DeleteStoredTrustTokens(
      const url::Origin& issuer,
      DeleteStoredTrustTokensCallback callback) override;
  void SetBlockTrustTokens(bool block) override;
  void ClearNetworkingHistoryBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceClosure completion_callback) override;
  void ClearHttpCache(base::Time start_time,
                      base::Time end_time,
                      mojom::ClearDataFilterPtr filter,
                      ClearHttpCacheCallback callback) override;
  void ComputeHttpCacheSize(base::Time start_time,
                            base::Time end_time,
                            ComputeHttpCacheSizeCallback callback) override;
  void NotifyExternalCacheHit(const GURL& url,
                              const std::string& http_method,
                              const net::NetworkIsolationKey& key,
                              bool include_credentials) override;
  void ClearCorsPreflightCache(
      mojom::ClearDataFilterPtr filter,
      ClearCorsPreflightCacheCallback callback) override;
  void ClearHostCache(mojom::ClearDataFilterPtr filter,
                      ClearHostCacheCallback callback) override;
  void ClearHttpAuthCache(base::Time start_time,
                          base::Time end_time,
                          mojom::ClearDataFilterPtr filter,
                          ClearHttpAuthCacheCallback callback) override;
  void ClearReportingCacheReports(
      mojom::ClearDataFilterPtr filter,
      ClearReportingCacheReportsCallback callback) override;
  void ClearReportingCacheClients(
      mojom::ClearDataFilterPtr filter,
      ClearReportingCacheClientsCallback callback) override;
  void ClearNetworkErrorLogging(
      mojom::ClearDataFilterPtr filter,
      ClearNetworkErrorLoggingCallback callback) override;
  void ClearDomainReliability(mojom::ClearDataFilterPtr filter,
                              DomainReliabilityClearMode mode,
                              ClearDomainReliabilityCallback callback) override;
  void CloseAllConnections(CloseAllConnectionsCallback callback) override;
  void CloseIdleConnections(CloseIdleConnectionsCallback callback) override;
  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            mojom::NetworkConditionsPtr conditions) override;
  void SetAcceptLanguage(const std::string& new_accept_language) override;
  void SetEnableReferrers(bool enable_referrers) override;
#if BUILDFLAG(IS_CT_SUPPORTED)
  void SetCTPolicy(mojom::CTPolicyPtr ct_policy) override;
  void MaybeEnqueueSCTReport(
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps);
  void SetSCTAuditingMode(mojom::SCTAuditingMode mode) override;
  SCTAuditingHandler* sct_auditing_handler() {
    return sct_auditing_handler_.get();
  }
  void CanSendSCTAuditingReport(base::OnceCallback<void(bool)> callback);
  void OnNewSCTAuditingReportSent();
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  void CreateUDPSocket(
      mojo::PendingReceiver<mojom::UDPSocket> receiver,
      mojo::PendingRemote<mojom::UDPSocketListener> listener) override;
  void CreateRestrictedUDPSocket(
      const net::IPEndPoint& addr,
      mojom::RestrictedUDPSocketMode mode,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::RestrictedUDPSocketParamsPtr params,
      mojo::PendingReceiver<mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<mojom::UDPSocketListener> listener,
      mojom::NetworkContext::CreateRestrictedUDPSocketCallback callback)
      override;
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      mojom::TCPServerSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
      CreateTCPServerSocketCallback callback) override;
  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override;
  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPBoundSocket> receiver,
      CreateTCPBoundSocketCallback callback) override;
  void CreateProxyResolvingSocketFactory(
      mojo::PendingReceiver<mojom::ProxyResolvingSocketFactory> receiver)
      override;
  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<mojom::ProxyLookupClient> proxy_lookup_client)
      override;
  void ForceReloadProxyConfig(ForceReloadProxyConfigCallback callback) override;
  void ClearBadProxiesCache(ClearBadProxiesCacheCallback callback) override;
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
      override;
  void CreateWebTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      std::vector<mojom::WebTransportCertificateFingerprintPtr> fingerprints,
      mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client)
      override;
  void CreateNetLogExporter(
      mojo::PendingReceiver<mojom::NetLogExporter> receiver) override;
  void ResolveHost(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<mojom::ResolveHostClient> response_client) override;
  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<mojom::HostResolver> receiver) override;
  void VerifyCertForSignedExchange(
      const scoped_refptr<net::X509Certificate>& certificate,
      const GURL& url,
      const std::string& ocsp_result,
      const std::string& sct_list,
      VerifyCertForSignedExchangeCallback callback) override;
  void AddHSTS(const std::string& host,
               base::Time expiry,
               bool include_subdomains,
               AddHSTSCallback callback) override;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override;
  void GetHSTSState(const std::string& domain,
                    GetHSTSStateCallback callback) override;
  void DeleteDynamicDataForHost(
      const std::string& host,
      DeleteDynamicDataForHostCallback callback) override;
  void SetCorsOriginAccessListsForOrigin(
      const url::Origin& source_origin,
      std::vector<mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<mojom::CorsOriginPatternPtr> block_patterns,
      SetCorsOriginAccessListsForOriginCallback callback) override;
  void EnableStaticKeyPinningForTesting(
      EnableStaticKeyPinningForTestingCallback callback) override;
  void VerifyCertificateForTesting(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& hostname,
      const std::string& ocsp_response,
      const std::string& sct_list,
      VerifyCertificateForTestingCallback callback) override;
  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      mojom::CredentialsMode credentials_mode,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;
#if BUILDFLAG(IS_P2P_ENABLED)
  void CreateP2PSocketManager(
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client,
      mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
          trusted_socket_manager,
      mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver)
      override;
#endif  // BUILDFLAG(IS_P2P_ENABLED)
  void CreateMdnsResponder(
      mojo::PendingReceiver<mojom::MdnsResponder> responder_receiver) override;
  void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) override;
  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override;
  void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) override;
  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::Value::Dict body) override;
  void QueueEnterpriseReport(const std::string& type,
                             const std::string& group,
                             const GURL& url,
                             base::Value::Dict body) override;
  void QueueSignedExchangeReport(
      mojom::SignedExchangeReportPtr report,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;
  void AddDomainReliabilityContextForTesting(
      const url::Origin& origin,
      const GURL& upload_url,
      AddDomainReliabilityContextForTestingCallback callback) override;
  void ForceDomainReliabilityUploadsForTesting(
      ForceDomainReliabilityUploadsForTestingCallback callback) override;
  void SetSplitAuthCacheByNetworkAnonymizationKey(
      bool split_auth_cache_by_network_anonymization_key) override;
  void SaveHttpAuthCacheProxyEntries(
      SaveHttpAuthCacheProxyEntriesCallback callback) override;
  void LoadHttpAuthCacheProxyEntries(
      const base::UnguessableToken& cache_key,
      LoadHttpAuthCacheProxyEntriesCallback callback) override;
  void AddAuthCacheEntry(
      const net::AuthChallengeInfo& challenge,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const net::AuthCredentials& credentials,
      AddAuthCacheEntryCallback callback) override;
  void SetCorsNonWildcardRequestHeadersSupport(bool value) override;
  // TODO(mmenke): Rename this method and update Mojo docs to make it clear this
  // doesn't give proxy auth credentials.
  void LookupServerBasicAuthCredentials(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      LookupServerBasicAuthCredentialsCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LookupProxyAuthCredentials(
      const net::ProxyServer& proxy_server,
      const std::string& auth_scheme,
      const std::string& realm,
      LookupProxyAuthCredentialsCallback callback) override;
#endif
  void SetSharedDictionaryCacheMaxSize(uint64_t cache_max_size) override;
  void ClearSharedDictionaryCache(
      base::Time start_time,
      base::Time end_time,
      mojom::ClearDataFilterPtr filter,
      ClearSharedDictionaryCacheCallback callback) override;
  void ClearSharedDictionaryCacheForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      ClearSharedDictionaryCacheForIsolationKeyCallback callback) override;
  void GetSharedDictionaryUsageInfo(
      GetSharedDictionaryUsageInfoCallback callback) override;
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      GetSharedDictionaryInfoCallback callback) override;
  void GetSharedDictionaryOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      GetSharedDictionaryOriginsBetweenCallback callback) override;
  void PreloadSharedDictionaryInfoForDocument(
      const std::vector<GURL>& urls,
      mojo::PendingReceiver<mojom::PreloadedSharedDictionaryInfoHandle>
          preload_handle) override;
  void HasPreloadedSharedDictionaryInfoForTesting(
      HasPreloadedSharedDictionaryInfoForTestingCallback callback) override;
  void ResourceSchedulerClientVisibilityChanged(
      const base::UnguessableToken& client_token,
      bool visible) override;
  void FlushCachedClientCertIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate) override;
  void FlushMatchingCachedClientCert(
      const scoped_refptr<net::X509Certificate>& certificate) override;
  void SetCookieDeprecationLabel(
      const std::optional<std::string>& label) override;
  void RevokeNetworkForNonces(const std::vector<base::UnguessableToken>& nonces,
                              RevokeNetworkForNoncesCallback callback) override;
  void ClearNonces(const std::vector<base::UnguessableToken>& nonces) override;
  void ExemptUrlFromNetworkRevocationForNonce(
      const GURL& exempted_url,
      const base::UnguessableToken& nonce,
      ExemptUrlFromNetworkRevocationForNonceCallback callback) override;
  void Prefetch(int32_t request_id,
                uint32_t options,
                const ResourceRequest& request,
                const net::MutableNetworkTrafficAnnotationTag&
                    traffic_annotation) override;

  void GetBoundNetworkForTesting(
      GetBoundNetworkForTestingCallback callback) override;

  // Destroys |request| when a proxy lookup completes.
  void OnProxyLookupComplete(ProxyLookupRequest* proxy_lookup_request);

  // Disables use of QUIC by the NetworkContext.
  void DisableQuic();

  // Destroys the specified factory. Called by the factory itself when it has
  // no open pipes.
  void DestroyURLLoaderFactory(
      PrefetchMatchingURLLoaderFactory* url_loader_factory);

  // Removes |transport| and destroys it.
  void Remove(WebTransport* transport);

  // The following methods are used to track the number of requests per process
  // and ensure it doesn't go over a reasonable limit.
  void LoaderCreated(uint32_t process_id);
  void LoaderDestroyed(uint32_t process_id);
  bool CanCreateLoader(uint32_t process_id);

  void set_max_loaders_per_process_for_testing(uint32_t count) {
    max_loaders_per_process_ = count;
  }

  size_t GetNumOutstandingResolveHostRequestsForTesting() const;

  size_t pending_proxy_lookup_requests_for_testing() const {
    return proxy_lookup_requests_.size();
  }

  void set_network_qualities_pref_delegate_for_testing(
      std::unique_ptr<NetworkQualitiesPrefDelegate>
          network_qualities_pref_delegate) {
    network_qualities_pref_delegate_ =
        std::move(network_qualities_pref_delegate);
  }

  cors::PreflightController* cors_preflight_controller() {
    return &cors_preflight_controller_;
  }

  // Returns true if reports should unconditionally be sent without first
  // consulting NetworkContextClient.OnCanSendReportingReports()
  bool SkipReportingPermissionCheck() const;

  // Creates a new url loader factory bound to this network context. For use
  // inside the network service.
  void CreateTrustedUrlLoaderFactoryForNetworkService(
      mojo::PendingReceiver<mojom::URLLoaderFactory>
          url_loader_factory_pending_receiver);

  domain_reliability::DomainReliabilityMonitor* domain_reliability_monitor() {
    return domain_reliability_monitor_.get();
  }

  // The http_auth_dynamic_params_ would be used to populate
  // the |http_auth_merged_preferences| of the given NetworkContext.
  void OnHttpAuthDynamicParamsChanged(
      const mojom::HttpAuthDynamicParams*
          http_auth_dynamic_network_service_params);

  const net::HttpAuthPreferences* GetHttpAuthPreferences() const;

  size_t NumOpenWebTransports() const;

  size_t num_url_loader_factories_for_testing() const {
    return url_loader_factories_.size();
  }

  // Returns whether all URLLoaderFactories owned by `this` are bound to
  // `bound_network`.
  bool AllURLLoaderFactoriesAreBoundToNetworkForTesting(
      net::handles::NetworkHandle bound_network) const;

  // Maintains Trust Tokens protocol state
  // (https://github.com/WICG/trust-token-api). Used by URLLoader to check
  // preconditions before annotating requests with protocol-related headers
  // and to store information conveyed in the corresponding responses.
  //
  // May return null if Trust Tokens support is disabled.
  PendingTrustTokenStore* trust_token_store() {
    return trust_token_store_.get();
  }
  const PendingTrustTokenStore* trust_token_store() const {
    return trust_token_store_.get();
  }
  bool are_trust_tokens_blocked() const { return block_trust_tokens_; }

  WebBundleManager& GetWebBundleManager() { return web_bundle_manager_; }

  SharedDictionaryManager* GetSharedDictionaryManager() {
    return shared_dictionary_manager_.get();
  }

  // Returns the current same-origin-policy exceptions.  For more details see
  // network::mojom::NetworkContextParams::cors_origin_access_list and
  // network::mojom::NetworkContext::SetCorsOriginAccessListsForOrigin.
  const cors::OriginAccessList& cors_origin_access_list() {
    return cors_origin_access_list_;
  }

  bool require_network_anonymization_key() const {
    return require_network_anonymization_key_;
  }

  bool acam_preflight_spec_conformant() const {
    return acam_preflight_spec_conformant_;
  }

  cors::NonWildcardRequestHeadersSupport
  cors_non_wildcard_request_headers_support() const {
    return cors_non_wildcard_request_headers_support_;
  }

  FirstPartySetsAccessDelegate& first_party_sets_access_delegate() {
    return first_party_sets_access_delegate_;
  }

#if BUILDFLAG(ENABLE_REPORTING)
  void AddReportingApiObserver(
      mojo::PendingRemote<network::mojom::ReportingApiObserver> observer)
      override;
  void OnReportAdded(const net::ReportingReport* service_report) override;
  void OnReportUpdated(const net::ReportingReport* service_report) override;
  void OnReportingObserverDisconnect(mojo::RemoteSetElementId mojo_id);
  void OnEndpointsUpdatedForOrigin(
      const std::vector<net::ReportingEndpoint>& endpoints) override;
#endif  // BUILDFLAG(ENABLE_REPORTING)

  // Checks whether network access for the partition nonce `nonce` and url
  // `url` is allowed. See `network_revocation_nonces_` and
  // `network_revocation_exemptions_`.
  bool IsNetworkForNonceAndUrlAllowed(const base::UnguessableToken& nonce,
                                      const GURL& url) const;

 private:
  class NetworkContextHttpAuthPreferences : public net::HttpAuthPreferences {
   public:
    explicit NetworkContextHttpAuthPreferences(NetworkService* network_service);
    ~NetworkContextHttpAuthPreferences() override;
#if BUILDFLAG(IS_LINUX)
    bool AllowGssapiLibraryLoad() const override;
#endif  // BUILDFLAG(IS_LINUX)
   private:
    const raw_ptr<NetworkService> network_service_;
  };

  // To be called back from CookieManager on settings change.
  void OnCookieManagerSettingsChanged();

  URLRequestContextOwner MakeURLRequestContext(
      mojo::PendingRemote<mojom::URLLoaderFactory>
          url_loader_factory_for_cert_net_fetcher,
      scoped_refptr<SessionCleanupCookieStore>,
      OnURLRequestContextBuilderConfiguredCallback
          on_url_request_context_builder_configured,
      net::handles::NetworkHandle bound_network);
  scoped_refptr<SessionCleanupCookieStore> MakeSessionCleanupCookieStore()
      const;

  // Invoked when the HTTP cache was cleared. Invokes |callback|.
  void OnHttpCacheCleared(ClearHttpCacheCallback callback,
                          HttpCacheDataRemover* remover);

  void OnHostResolverShutdown(HostResolver* resolver);

  // Invoked when the computation for ComputeHttpCacheSize() has been completed,
  // to report result to user via |callback| and clean things up.
  void OnHttpCacheSizeComputed(ComputeHttpCacheSizeCallback callback,
                               HttpCacheDataCounter* counter,
                               bool is_upper_limit,
                               int64_t result_or_error);

  // On connection errors the NetworkContext destroys itself.
  void OnConnectionError();

  // On disconnect of owned RCMs references need to be cleaned up.
  void OnRCMDisconnect(const network::RestrictedCookieManager* rcm);

  // Invoked with the FirstPartySetMetadata to be associated with the given
  // RestrictedCookieManager that is being set up.
  void OnComputedFirstPartySetMetadata(
      mojo::PendingReceiver<mojom::RestrictedCookieManager> receiver,
      mojom::RestrictedCookieManagerRole role,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
      net::FirstPartySetMetadata first_party_set_metadata);

  GURL GetHSTSRedirect(const GURL& original_url);

#if BUILDFLAG(IS_P2P_ENABLED)
  void DestroySocketManager(P2PSocketManager* socket_manager);
#endif  // BUILDFLAG(IS_P2P_ENABLED)

  void CanUploadDomainReliability(const url::Origin& origin,
                                  base::OnceCallback<void(bool)> callback);

  void OnVerifyCertForSignedExchangeComplete(uint64_t cert_verify_id,
                                             int result);

#if BUILDFLAG(IS_CHROMEOS)
  void TrustAnchorUsed();
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
  // Checks the Certificate Transparency policy compliance for a given
  // certificate and SCTs in `cert_verify_result`, and updates
  // `cert_verify_result.cert_status` and
  // `cert_verify_result.policy_compliance`. Returns net::OK or
  // net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED.
  // TODO(crbug.com/41380502): This code is more-or-less duplicated in
  // SSLClientSocket and QUIC. Fold this into some CertVerifier-shaped class
  // in //net.
  int CheckCTRequirementsForSignedExchange(
      net::CertVerifyResult& cert_verify_result,
      const net::HostPortPair& host_port_pair);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
  void EnsureMounted(network::TransferableDirectory* directory);
#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

  void InitializeCorsParams();

  // If |trust_token_store_| is backed by an asynchronously-constructed (e.g.,
  // SQL-based) persistence layer, |FinishConstructingTrustTokenStore|
  // constructs and populates |trust_token_store_| once the persister's
  // asynchronous initialization has finished.
  void FinishConstructingTrustTokenStore(
      std::unique_ptr<SQLiteTrustTokenPersister> persister);

  bool IsAllowedToUseAllHttpAuthSchemes(
      const url::SchemeHostPort& scheme_host_port);

  void InitializePrefetchURLLoaderFactory();

  void QueueReportInternal(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::Value::Dict body,
      net::ReportingTargetType target_type);

  const raw_ptr<NetworkService> network_service_;

  mojo::Remote<mojom::NetworkContextClient> client_;

  std::unique_ptr<ResourceScheduler> resource_scheduler_;

  // Used only when network::features::kCompressionDictionaryTransportBackend is
  // enabled.
  // Note: `url_request_context_owner_` indirectly holds a pointer to
  // `shared_dictionary_manager_` via URLRequestContext and HttpCache and
  // SharedDictionaryNetworkTransactionFactory. So `url_request_context_owner_`
  // needs to be defined after `shared_dictionary_manager_`.
  std::unique_ptr<SharedDictionaryManager> shared_dictionary_manager_;

  std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
      domain_reliability_monitor_;

  // Holds owning pointer to |url_request_context_|. Will contain a nullptr for
  // |url_request_context| when the NetworkContextImpl doesn't own its own
  // URLRequestContext.
  URLRequestContextOwner url_request_context_owner_;

  raw_ptr<net::URLRequestContext> url_request_context_;

#if BUILDFLAG(ENABLE_REPORTING)
  bool is_observing_reporting_service_;
  mojo::RemoteSet<network::mojom::ReportingApiObserver>
      reporting_api_observers_;
#endif  // BUILDFLAG(ENABLE_REPORTING)

  // Owned by URLRequestContext.
  raw_ptr<NetworkServiceNetworkDelegate> network_delegate_ = nullptr;

  mojom::NetworkContextParamsPtr params_;

  // These must be below the URLRequestContext, so they're destroyed before it
  // is.
  // These should also be above receiver_ so the bindings are destroyed prior to
  // the callbacks themselves.
  std::vector<std::unique_ptr<HttpCacheDataRemover>> http_cache_data_removers_;
  std::vector<std::unique_ptr<HttpCacheDataCounter>> http_cache_data_counters_;
  std::set<std::unique_ptr<ProxyLookupRequest>, base::UniquePtrComparator>
      proxy_lookup_requests_;

  // If non-null, called when the mojo pipe for the NetworkContext is closed.
  OnConnectionCloseCallback on_connection_close_callback_;

#if BUILDFLAG(IS_ANDROID)
  std::vector<std::unique_ptr<base::android::ApplicationStatusListener>>
      app_status_listeners_;
#endif

  mojo::Receiver<mojom::NetworkContext> receiver_;

  FirstPartySetsAccessDelegate first_party_sets_access_delegate_;

  std::unique_ptr<CookieManager> cookie_manager_;

  std::unique_ptr<SocketFactory> socket_factory_;

  mojo::UniqueReceiverSet<mojom::ProxyResolvingSocketFactory>
      proxy_resolving_socket_factories_;

  // See the comment for |trust_token_store()|.
  std::unique_ptr<PendingTrustTokenStore> trust_token_store_;

  // Ordering: this must be after |trust_token_store_| since the
  // TrustTokenQueryAnswerers are provided non-owning pointers to
  // |trust_token_store_|.
  mojo::UniqueReceiverSet<mojom::TrustTokenQueryAnswerer>
      trust_token_query_answerers_;

  // Whether the user is blocking Trust Tokens, value provided by the
  // PrivacySandboxSettings service.
  bool block_trust_tokens_ = false;

#if BUILDFLAG(ENABLE_WEBSOCKETS)
  std::unique_ptr<WebSocketFactory> websocket_factory_;
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

  std::set<std::unique_ptr<WebTransport>, base::UniquePtrComparator>
      web_transports_;

  // A count of outstanding requests per initiating process.
  std::map<uint32_t, uint32_t> loader_count_per_process_;

  static constexpr uint32_t kMaxOutstandingRequestsPerProcess = 2700;
  uint32_t max_loaders_per_process_ = kMaxOutstandingRequestsPerProcess;

#if BUILDFLAG(IS_P2P_ENABLED)
  base::flat_map<P2PSocketManager*, std::unique_ptr<P2PSocketManager>>
      socket_managers_;
#endif  // BUILDFLAG(IS_P2P_ENABLED)

#if BUILDFLAG(ENABLE_MDNS)
  std::unique_ptr<MdnsResponderManager> mdns_responder_manager_;
#endif  // BUILDFLAG(ENABLE_MDNS)

  mojo::UniqueReceiverSet<mojom::NetLogExporter> net_log_exporter_receivers_;

  // Ordering: this must be after |cookie_manager_| since members point to its
  // CookieSettings object.
  std::set<std::unique_ptr<network::RestrictedCookieManager>,
           base::UniquePtrComparator>
      restricted_cookie_managers_;

  // Owned by the URLRequestContext
  raw_ptr<net::StaticHttpUserAgentSettings> user_agent_settings_ = nullptr;

#if BUILDFLAG(IS_CT_SUPPORTED)
  std::unique_ptr<certificate_transparency::ChromeRequireCTDelegate>
      require_ct_delegate_;

  std::unique_ptr<SCTAuditingHandler> sct_auditing_handler_;
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
  // Contains a list of closures that, when run, will dismount the shared
  // directories used by this NetworkClosure.
  std::vector<base::OnceClosure> dismount_closures_;
#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

  // Created on-demand. Null if unused.
  std::unique_ptr<HostResolver> internal_host_resolver_;
  std::set<std::unique_ptr<HostResolver>, base::UniquePtrComparator>
      host_resolvers_;
  std::unique_ptr<net::HostResolver::ProbeRequest> doh_probes_request_;

  // Used for Signed Exchange certificate verification.
  uint64_t next_cert_verify_id_ = 0;
  struct PendingCertVerify {
    PendingCertVerify();
    ~PendingCertVerify();

    // CertVerifyResult must be freed after the Request has been destructed.
    // So |result| must be written before |request|.
    std::unique_ptr<net::CertVerifyResult> result;
    std::unique_ptr<net::CertVerifier::Request> request;
    VerifyCertForSignedExchangeCallback callback;
    scoped_refptr<net::X509Certificate> certificate;
    GURL url;
    std::string ocsp_result;
    std::string sct_list;
  };
  std::map<uint64_t, std::unique_ptr<PendingCertVerify>>
      cert_verifier_requests_;

  // Manages allowed origin access lists.
  cors::OriginAccessList cors_origin_access_list_;

  // Manages header keys that are allowed to be used in
  // ResourceRequest::cors_exempt_headers.
  base::flat_set<std::string> cors_exempt_header_list_;

  // Manages CORS preflight requests and its cache.
  cors::PreflightController cors_preflight_controller_;

  std::unique_ptr<NetworkQualitiesPrefDelegate>
      network_qualities_pref_delegate_;

  // Each network context holds its own HttpAuthPreferences.
  // The dynamic preferences of |NetworkService| and the static
  // preferences from |NetworkContext| would be merged to
  // `http_auth_merged_preferences_` which would then be used to create
  // HttpAuthHandle via |NetworkContext::CreateHttpAuthHandlerFactory|.
  NetworkContextHttpAuthPreferences http_auth_merged_preferences_;

  // Each network context holds its own WebBundleManager, which
  // manages the lifetiem of a WebBundleURLLoaderFactory object.
  WebBundleManager web_bundle_manager_;

  // The ohttp_handler_ needs to be destroyed before cookie_manager_, since it
  // depends on it indirectly through this context.
  ObliviousHttpRequestHandler ohttp_handler_;

  // Whether all external consumers are expected to provide a non-empty
  // NetworkAnonymizationKey with all requests. When set, enabled a variety of
  // DCHECKs on APIs used by external callers.
  bool require_network_anonymization_key_ = false;

  // Whether Access-Control-Allow-Methods matching in CORS preflight is done
  // according to the spec.
  bool acam_preflight_spec_conformant_ = true;

  // True once the destructor has been called. Used to guard against re-entrant
  // calls to DestroyURLLoaderFactory().
  bool is_destructing_ = false;

  // True if the Prefetch() method is enabled.
  // TODO(ricea): Remove this when it is enabled by default.
  const bool prefetch_enabled_;

  // Indicating whether
  // https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name is
  // supported.
  cors::NonWildcardRequestHeadersSupport
      cors_non_wildcard_request_headers_support_;

  // CorsURLLoaderFactory assumes that fields owned by the NetworkContext always
  // live longer than the factory.  Therefore we want the factories to be
  // destroyed before other fields above.  This is accomplished by explicitly
  // clearing `url_loader_factories_` in the destructor.
  std::set<std::unique_ptr<PrefetchMatchingURLLoaderFactory>,
           base::UniquePtrComparator>
      url_loader_factories_;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;

  scoped_refptr<MojoBackendFileOperationsFactory>
      http_cache_file_operations_factory_;

  // A data structure that tracks partition nonces whose network requests
  // should be blocked, for fenced frames network revocation.
  // https://github.com/WICG/fenced-frame/blob/master/explainer/fenced_frames_with_local_unpartitioned_data_access.md#revoking-network-access
  // New nonces are inserted by `RevokeNetworkForNonce`,
  // and membership is checked with `IsNetworkForNonceAndUrlAllowed`.
  std::set<base::UnguessableToken> network_revocation_nonces_;

  // A data structure that tracks urls that should be exempted from network
  // revocation, to facilitate testing.
  // New urls are inserted by
  // `ExemptUrlFromNetworkRevocationForNonce`
  // and membership is checked with `IsNetworkForNonceAndUrlAllowed`.
  std::map<base::UnguessableToken, std::set<GURL>>
      network_revocation_exemptions_;

  // An LRU cache for in-progress prefetches. Created on first use.
  std::unique_ptr<PrefetchCache> prefetch_cache_;

  // The URLLoaderFactory to use for prefetches. Created on first use.
  mojo::Remote<mojom::URLLoaderFactory> prefetch_url_loader_factory_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NetworkContext> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_CONTEXT_H_
