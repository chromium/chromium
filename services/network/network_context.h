// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_CONTEXT_H_
#define SERVICES_NETWORK_NETWORK_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/host_resolver.h"
#include "services/network/http_cache_data_counter.h"
#include "services/network/http_cache_data_remover.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/socket_factory.h"
#include "services/network/url_request_context_owner.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace net {
class CertVerifier;
class HostPortPair;
class ReportSender;
class StaticHttpUserAgentSettings;
class URLRequestContext;
}  // namespace net

namespace certificate_transparency {
class ChromeRequireCTDelegate;
class TreeStateTracker;
}  // namespace certificate_transparency

namespace network {
class CertVerifierWithTrustAnchors;
class CookieManager;
class ExpectCTReporter;
class HostResolver;
class NetworkService;
class NetworkServiceProxyDelegate;
class P2PSocketManager;
class ProxyLookupRequest;
class ResourceScheduler;
class ResourceSchedulerClient;
class URLRequestContextBuilderMojo;
class WebSocketFactory;

namespace cors {
class CORSURLLoaderFactory;
}  // namespace cors

// A NetworkContext creates and manages access to a URLRequestContext.
//
// When the network service is enabled, NetworkContexts are created through
// NetworkService's mojo interface and are owned jointly by the NetworkService
// and the NetworkContextPtr used to talk to them, and the NetworkContext is
// destroyed when either one is torn down.
//
// When the network service is disabled, NetworkContexts may be created through
// NetworkService::CreateNetworkContextWithBuilder, and take in a
// URLRequestContextBuilderMojo to seed construction of the NetworkContext's
// URLRequestContext. When that happens, the consumer takes ownership of the
// NetworkContext directly, has direct access to its URLRequestContext, and is
// responsible for destroying it before the NetworkService.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkContext
    : public mojom::NetworkContext {
 public:
  using OnConnectionCloseCallback =
      base::OnceCallback<void(NetworkContext* network_context)>;

  NetworkContext(NetworkService* network_service,
                 mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params,
                 OnConnectionCloseCallback on_connection_close_callback =
                     OnConnectionCloseCallback());

  // DEPRECATED: Creates an in-process NetworkContext with a partially
  // pre-populated URLRequestContextBuilderMojo. This API should not be used
  // in new code, as some |params| configuration may be ignored, in favor of
  // the pre-configured URLRequestContextBuilderMojo configuration.
  NetworkContext(NetworkService* network_service,
                 mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params,
                 std::unique_ptr<URLRequestContextBuilderMojo> builder);

  // DEPRECATED: Creates a NetworkContext that simply wraps a consumer-provided
  // URLRequestContext that is not owned by the NetworkContext.
  // TODO(mmenke):  Remove this constructor when the network service ships.
  NetworkContext(NetworkService* network_service,
                 mojom::NetworkContextRequest request,
                 net::URLRequestContext* url_request_context);

  ~NetworkContext() override;

  // Sets a global CertVerifier to use when initializing all profiles.
  static void SetCertVerifierForTesting(net::CertVerifier* cert_verifier);

  // Whether the NetworkContext should be used for certain URL fetches of
  // global scope (validating certs on some platforms, DNS over HTTPS).
  // May only be set to true the first NetworkContext created using the
  // NetworkService.  Destroying the NetworkContext with this set to true
  // will destroy all other NetworkContexts.
  bool IsPrimaryNetworkContext() const;

  net::URLRequestContext* url_request_context() { return url_request_context_; }

  NetworkService* network_service() { return network_service_; }

  mojom::NetworkContextClient* client() { return client_.get(); }

  ResourceScheduler* resource_scheduler() { return resource_scheduler_.get(); }

  CookieManager* cookie_manager() { return cookie_manager_.get(); }

#if defined(OS_ANDROID)
  base::android::ApplicationStatusListener* app_status_listener() const {
    return app_status_listener_.get();
  }
#endif

  // Creates a URLLoaderFactory with a ResourceSchedulerClient specified. This
  // is used to reuse the existing ResourceSchedulerClient for cloned
  // URLLoaderFactory.
  void CreateURLLoaderFactory(
      mojom::URLLoaderFactoryRequest request,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client);

  // mojom::NetworkContext implementation:
  void SetClient(mojom::NetworkContextClientPtr client) override;
  void CreateURLLoaderFactory(mojom::URLLoaderFactoryRequest request,
                              mojom::URLLoaderFactoryParamsPtr params) override;
  void GetCookieManager(mojom::CookieManagerRequest request) override;
  void GetRestrictedCookieManager(mojom::RestrictedCookieManagerRequest request,
                                  const url::Origin& origin) override;
  void ClearNetworkingHistorySince(
      base::Time time,
      base::OnceClosure completion_callback) override;
  void ClearHttpCache(base::Time start_time,
                      base::Time end_time,
                      mojom::ClearDataFilterPtr filter,
                      ClearHttpCacheCallback callback) override;
  void ComputeHttpCacheSize(base::Time start_time,
                            base::Time end_time,
                            ComputeHttpCacheSizeCallback callback) override;
  void ClearChannelIds(base::Time start_time,
                       base::Time end_time,
                       mojom::ClearDataFilterPtr filter,
                       ClearChannelIdsCallback callback) override;
  void ClearHostCache(mojom::ClearDataFilterPtr filter,
                      ClearHostCacheCallback callback) override;
  void ClearHttpAuthCache(base::Time start_time,
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
  void CloseAllConnections(CloseAllConnectionsCallback callback) override;
  void CloseIdleConnections(CloseIdleConnectionsCallback callback) override;
  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            mojom::NetworkConditionsPtr conditions) override;
  void SetAcceptLanguage(const std::string& new_accept_language) override;
  void SetEnableReferrers(bool enable_referrers) override;
  void SetCTPolicy(
      const std::vector<std::string>& required_hosts,
      const std::vector<std::string>& excluded_hosts,
      const std::vector<std::string>& excluded_spkis,
      const std::vector<std::string>& excluded_legacy_spkis) override;
#if defined(OS_CHROMEOS)
  void UpdateTrustAnchors(const net::CertificateList& trust_anchors) override;
#endif
  void AddExpectCT(const std::string& domain,
                   base::Time expiry,
                   bool enforce,
                   const GURL& report_uri,
                   AddExpectCTCallback callback) override;
  void SetExpectCTTestReport(const GURL& report_uri,
                             SetExpectCTTestReportCallback callback) override;
  void GetExpectCTState(const std::string& domain,
                        GetExpectCTStateCallback callback) override;
  void CreateUDPSocket(mojom::UDPSocketRequest request,
                       mojom::UDPSocketReceiverPtr receiver) override;
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPServerSocketRequest request,
      CreateTCPServerSocketCallback callback) override;
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPConnectedSocketRequest request,
      mojom::SocketObserverPtr observer,
      CreateTCPConnectedSocketCallback callback) override;
  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPBoundSocketRequest request,
      CreateTCPBoundSocketCallback callback) override;
  void CreateProxyResolvingSocketFactory(
      mojom::ProxyResolvingSocketFactoryRequest request) override;
  void CreateWebSocket(mojom::WebSocketRequest request,
                       int32_t process_id,
                       int32_t render_frame_id,
                       const url::Origin& origin,
                       mojom::AuthenticationHandlerPtr auth_handler) override;
  void LookUpProxyForURL(
      const GURL& url,
      mojom::ProxyLookupClientPtr proxy_lookup_client) override;
  void ForceReloadProxyConfig(ForceReloadProxyConfigCallback callback) override;
  void ClearBadProxiesCache(ClearBadProxiesCacheCallback callback) override;
  void CreateNetLogExporter(mojom::NetLogExporterRequest request) override;
  void ResolveHost(const net::HostPortPair& host,
                   mojom::ResolveHostParametersPtr optional_parameters,
                   mojom::ResolveHostClientPtr response_client) override;
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojom::HostResolverRequest request) override;
  void WriteCacheMetadata(const GURL& url,
                          net::RequestPriority priority,
                          base::Time expected_response_time,
                          const std::vector<uint8_t>& data) override;
  void VerifyCertForSignedExchange(
      const scoped_refptr<net::X509Certificate>& certificate,
      const GURL& url,
      const std::string& ocsp_result,
      const std::string& sct_list,
      VerifyCertForSignedExchangeCallback callback) override;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override;
  void AddHSTS(const std::string& host,
               base::Time expiry,
               bool include_subdomains,
               AddHSTSCallback callback) override;
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
  void SetFailingHttpTransactionForTesting(
      int32_t rv,
      SetFailingHttpTransactionForTestingCallback callback) override;
  void VerifyCertificateForTesting(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& hostname,
      const std::string& ocsp_response,
      VerifyCertificateForTestingCallback callback) override;
  void PreconnectSockets(uint32_t num_streams,
                         const GURL& url,
                         int32_t load_flags,
                         bool privacy_mode_enabled) override;
  void CreateP2PSocketManager(
      mojom::P2PTrustedSocketManagerClientPtr client,
      mojom::P2PTrustedSocketManagerRequest trusted_socket_manager,
      mojom::P2PSocketManagerRequest socket_manager_request) override;
  void ResetURLLoaderFactories() override;

  // Destroys |request| when a proxy lookup completes.
  void OnProxyLookupComplete(ProxyLookupRequest* proxy_lookup_request);

  // Disables use of QUIC by the NetworkContext.
  void DisableQuic();

  // Destroys the specified factory. Called by the factory itself when it has
  // no open pipes.
  void DestroyURLLoaderFactory(cors::CORSURLLoaderFactory* url_loader_factory);

  size_t GetNumOutstandingResolveHostRequestsForTesting() const;

  size_t pending_proxy_lookup_requests_for_testing() const {
    return proxy_lookup_requests_.size();
  }

  NetworkServiceProxyDelegate* proxy_delegate() const {
    return proxy_delegate_.get();
  }

  void set_host_resolver_factory_for_testing(
      std::unique_ptr<net::HostResolver::Factory> factory) {
    host_resolver_factory_ = std::move(factory);
  }

 private:
  class ContextNetworkDelegate;

  // Applies the values in |params_| to |builder|, and builds the
  // URLRequestContext.
  URLRequestContextOwner ApplyContextParamsToBuilder(
      URLRequestContextBuilderMojo* builder);

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

  URLRequestContextOwner MakeURLRequestContext();

  GURL GetHSTSRedirect(const GURL& original_url);

  void DestroySocketManager(P2PSocketManager* socket_manager);

  void OnCertVerifyForSignedExchangeComplete(int cert_verify_id, int result);

#if defined(OS_CHROMEOS)
  void TrustAnchorUsed();
#endif

  void OnSetExpectCTTestReportSuccess();

  void LazyCreateExpectCTReporter(net::URLRequestContext* url_request_context);

  void OnSetExpectCTTestReportFailure();

  NetworkService* const network_service_;

  mojom::NetworkContextClientPtr client_;

  std::unique_ptr<ResourceScheduler> resource_scheduler_;

  // Holds owning pointer to |url_request_context_|. Will contain a nullptr for
  // |url_request_context| when the NetworkContextImpl doesn't own its own
  // URLRequestContext.
  URLRequestContextOwner url_request_context_owner_;

  net::URLRequestContext* url_request_context_;

  // Owned by URLRequestContext.
  ContextNetworkDelegate* context_network_delegate_ = nullptr;

  mojom::NetworkContextParamsPtr params_;

  // If non-null, called when the mojo pipe for the NetworkContext is closed.
  OnConnectionCloseCallback on_connection_close_callback_;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
#endif

  mojo::Binding<mojom::NetworkContext> binding_;

  std::unique_ptr<CookieManager> cookie_manager_;

  std::unique_ptr<SocketFactory> socket_factory_;

  mojo::StrongBindingSet<mojom::ProxyResolvingSocketFactory>
      proxy_resolving_socket_factories_;

#if !defined(OS_IOS)
  std::unique_ptr<WebSocketFactory> websocket_factory_;
#endif  // !defined(OS_IOS)

  // These must be below the URLRequestContext, so they're destroyed before it
  // is.
  std::vector<std::unique_ptr<HttpCacheDataRemover>> http_cache_data_removers_;
  std::vector<std::unique_ptr<HttpCacheDataCounter>> http_cache_data_counters_;
  std::set<std::unique_ptr<ProxyLookupRequest>, base::UniquePtrComparator>
      proxy_lookup_requests_;

  // This must be below |url_request_context_| so that the URLRequestContext
  // outlives all the URLLoaderFactories and URLLoaders that depend on it.
  std::set<std::unique_ptr<cors::CORSURLLoaderFactory>,
           base::UniquePtrComparator>
      url_loader_factories_;

  base::flat_map<P2PSocketManager*, std::unique_ptr<P2PSocketManager>>
      socket_managers_;

  mojo::StrongBindingSet<mojom::NetLogExporter> net_log_exporter_bindings_;

  mojo::StrongBindingSet<mojom::RestrictedCookieManager>
      restricted_cookie_manager_bindings_;

  int current_resource_scheduler_client_id_ = 0;

  // Owned by the URLRequestContext
  net::StaticHttpUserAgentSettings* user_agent_settings_ = nullptr;

  // TODO(yhirano): Consult with switches::kDisableResourceScheduler.
  constexpr static bool enable_resource_scheduler_ = true;

  // Pointed to by the TransportSecurityState (owned by the
  // URLRequestContext), and must be disconnected from it before it's destroyed.
  std::unique_ptr<net::ReportSender> certificate_report_sender_;

  std::unique_ptr<ExpectCTReporter> expect_ct_reporter_;

  std::unique_ptr<certificate_transparency::ChromeRequireCTDelegate>
      require_ct_delegate_;
  std::unique_ptr<certificate_transparency::TreeStateTracker> ct_tree_tracker_;

#if defined(OS_CHROMEOS)
  CertVerifierWithTrustAnchors* cert_verifier_with_trust_anchors_ = nullptr;
#endif

  // Created on-demand. Null if unused.
  std::unique_ptr<HostResolver> internal_host_resolver_;
  // Map values set to non-null only if that HostResolver has its own private
  // internal net::HostResolver.
  std::map<std::unique_ptr<HostResolver>,
           std::unique_ptr<net::HostResolver>,
           base::UniquePtrComparator>
      host_resolvers_;
  // Factory used to create any needed private internal net::HostResolvers.
  std::unique_ptr<net::HostResolver::Factory> host_resolver_factory_;

  std::unique_ptr<NetworkServiceProxyDelegate> proxy_delegate_;

  // Used for Signed Exchange certificate verification.
  int next_cert_verify_id_ = 0;
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
  std::map<int, std::unique_ptr<PendingCertVerify>> cert_verifier_requests_;

  // Manages allowed origin access lists.
  cors::OriginAccessList cors_origin_access_list_;

  std::queue<SetExpectCTTestReportCallback>
      outstanding_set_expect_ct_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_CONTEXT_H_
