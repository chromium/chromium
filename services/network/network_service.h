// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_H_

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
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/schemeful_site.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/trace_net_log_observer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/keepalive_statistics_recorder.h"
#include "services/network/network_change_manager.h"
#include "services/network/network_quality_estimator_manager.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/key_pinning.mojom.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_annotation_monitor.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_quality_estimator_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/system_dns_resolution.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/tpcd/metadata/manager.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

namespace mojo_base {
class ProtoWrapper;
}

namespace net {
class FileNetLogObserver;
class HostResolverManager;
class HttpAuthHandlerFactory;
class LoggingNetworkChangeObserver;
class NetworkChangeNotifier;
class NetworkQualityEstimator;
class URLRequestContext;
}  // namespace net

namespace network {

class DnsConfigChangeManager;
class HttpAuthCacheCopier;
class NetLogProxySink;
class NetworkContext;
class NetworkService;
class SCTAuditingCache;

class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkService
    : public mojom::NetworkService {
 public:
  static const base::TimeDelta kInitialDohProbeTimeout;

  explicit NetworkService(
      std::unique_ptr<service_manager::BinderRegistry> registry,
      mojo::PendingReceiver<mojom::NetworkService> receiver =
          mojo::NullReceiver(),
      bool delay_initialization_until_set_client = false);

  NetworkService(const NetworkService&) = delete;
  NetworkService& operator=(const NetworkService&) = delete;

  ~NetworkService() override;

  // Allows late binding if the mojo receiver wasn't specified in the
  // constructor.
  void Bind(mojo::PendingReceiver<mojom::NetworkService> receiver);

  // Allows the browser process to synchronously initialize the NetworkService.
  // TODO(jam): remove this once the old path is gone.
  void Initialize(mojom::NetworkServiceParamsPtr params,
                  bool mock_network_change_notifier = false);

  // Pretends that the system DNS configuration just changed to a basic,
  // single-server, localhost-only configuration. This method also effectively
  // unsubscribes the singleton `net::SystemDnsConfigChangeNotifier` owned by
  // `net::NetworkChangeNotifier` from future changes to the real configuration,
  // ensuring that our fake configuration will not be clobbered by network
  // changes that occur while tests run.
  // Once this is finished, `replace_cb` will run.
  void ReplaceSystemDnsConfigForTesting(base::OnceClosure replace_cb);

  void SetTestDohConfigForTesting(net::SecureDnsMode secure_dns_mode,
                                  const net::DnsOverHttpsConfig& doh_config);

  // Creates a NetworkService instance on the current thread.
  static std::unique_ptr<NetworkService> Create(
      mojo::PendingReceiver<mojom::NetworkService> receiver);

  // Creates a testing instance of NetworkService not bound to an actual
  // Service pipe. This instance must be driven by direct calls onto the
  // NetworkService object.
  static std::unique_ptr<NetworkService> CreateForTesting();

  // These are called by NetworkContexts as they are being created and
  // destroyed.
  // TODO(mmenke): Remove once all NetworkContexts are owned by the
  // NetworkService.
  void RegisterNetworkContext(NetworkContext* network_context);
  void DeregisterNetworkContext(NetworkContext* network_context);

  // Invokes net::CreateNetLogEntriesForActiveObjects(observer) on all
  // URLRequestContext's known to |this|.
  void CreateNetLogEntriesForActiveObjects(
      net::NetLog::ThreadSafeObserver* observer);

  void SetNetworkAnnotationMonitor(
      mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor> remote)
      override;

  void NotifyNetworkRequestWithAnnotation(
      net::NetworkTrafficAnnotationTag traffic_annotation);

  // mojom::NetworkService implementation:
  void SetParams(mojom::NetworkServiceParamsPtr params) override;
  void StartNetLog(base::File file,
                   uint64_t max_total_size,
                   net::NetLogCaptureMode capture_mode,
                   base::Value::Dict constants) override;
  void AttachNetLogProxy(
      mojo::PendingRemote<mojom::NetLogProxySource> proxy_source,
      mojo::PendingReceiver<mojom::NetLogProxySink>) override;
  void SetSSLKeyLogFile(base::File file) override;
  void CreateNetworkContext(
      mojo::PendingReceiver<mojom::NetworkContext> receiver,
      mojom::NetworkContextParamsPtr params) override;
  void ConfigureStubHostResolver(
      bool insecure_dns_client_enabled,
      net::SecureDnsMode secure_dns_mode,
      const net::DnsOverHttpsConfig& dns_over_https_config,
      bool additional_dns_types_enabled) override;
  void DisableQuic() override;
  void SetUpHttpAuth(
      mojom::HttpAuthStaticParamsPtr http_auth_static_params) override;
  void ConfigureHttpAuthPrefs(
      mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) override;
  void SetRawHeadersAccess(int32_t process_id,
                           const std::vector<url::Origin>& origins) override;
  void SetMaxConnectionsPerProxyChain(int32_t max_connections) override;
  void GetNetworkChangeManager(
      mojo::PendingReceiver<mojom::NetworkChangeManager> receiver) override;
  void GetNetworkQualityEstimatorManager(
      mojo::PendingReceiver<mojom::NetworkQualityEstimatorManager> receiver)
      override;
  void GetDnsConfigChangeManager(
      mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver) override;
  void GetNetworkList(
      uint32_t policy,
      mojom::NetworkService::GetNetworkListCallback callback) override;
  void OnTrustStoreChanged() override;
  void OnClientCertStoreChanged() override;
  void SetEncryptionKey(const std::string& encryption_key) override;
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel
                            memory_pressure_level) override;
  void OnPeerToPeerConnectionsCountChange(uint32_t count) override;
#if BUILDFLAG(IS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state) override;
#endif  // BUILDFLAG(IS_ANDROID)
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   base::OnceClosure done) override;
  void ParseHeaders(const GURL& url,
                    const scoped_refptr<net::HttpResponseHeaders>& headers,
                    ParseHeadersCallback callback) override;
  void EnableDataUseUpdates(bool enable) override;
  void SetIPv6ReachabilityOverride(bool reachability_override) override;
#if BUILDFLAG(IS_CT_SUPPORTED)
  void ClearSCTAuditingCache() override;
  void ConfigureSCTAuditing(
      mojom::SCTAuditingConfigurationPtr configuration) override;
  void UpdateCtLogList(std::vector<mojom::CTLogInfoPtr> log_list,
                       UpdateCtLogListCallback callback) override;
  void UpdateCtKnownPopularSCTs(
      const std::vector<std::vector<uint8_t>>& sct_hashes,
      UpdateCtKnownPopularSCTsCallback callback) override;
  void SetCtEnforcementEnabled(
      bool enabled,
      SetCtEnforcementEnabledCallback callback) override;
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  void UpdateKeyPinsList(mojom::PinListPtr pin_list,
                         base::Time update_time) override;

  void UpdateMaskedDomainList(
      mojo_base::ProtoWrapper masked_domain_list,
      const std::vector<std::string>& exclusion_list) override;

#if BUILDFLAG(IS_ANDROID)
  void DumpWithoutCrashing(base::Time dump_request_time) override;
#endif  // BUILDFLAG(IS_ANDROID)
  void BindTestInterfaceForTesting(
      mojo::PendingReceiver<mojom::NetworkServiceTest> receiver) override;
  void SetFirstPartySets(net::GlobalFirstPartySets sets) override;

  void SetTpcdMetadataGrants(
      const std::vector<ContentSettingPatternSource>& settings) override;

  void SetExplicitlyAllowedPorts(const std::vector<uint16_t>& ports) override;
#if BUILDFLAG(IS_LINUX)
  void SetGssapiLibraryLoadObserver(
      mojo::PendingRemote<mojom::GssapiLibraryLoadObserver>
          gssapi_library_load_observer) override;
#endif  // BUILDFLAG(IS_LINUX)
  void StartNetLogBounded(base::File file,
                          uint64_t max_total_size,
                          net::NetLogCaptureMode capture_mode,
                          base::Value::Dict client_constants);

  // Called after StartNetLogBounded() finishes creating a scratch dir.
  void OnStartNetLogBoundedScratchDirectoryCreated(
      base::File file,
      uint64_t max_total_size,
      net::NetLogCaptureMode capture_mode,
      base::Value::Dict constants,
      const base::FilePath& in_progress_dir_path);

  void StartNetLogUnbounded(base::File file,
                            net::NetLogCaptureMode capture_mode,
                            base::Value::Dict client_constants);

  // Returns an HttpAuthHandlerFactory for the given NetworkContext.
  std::unique_ptr<net::HttpAuthHandlerFactory> CreateHttpAuthHandlerFactory(
      NetworkContext* network_context);

#if BUILDFLAG(IS_LINUX)
  // This is called just before a GSSAPI library may be loaded.
  void OnBeforeGssapiLibraryLoad();
#endif  // BUILDFLAG(IS_LINUX)

  bool quic_disabled() const { return quic_disabled_; }
  bool HasRawHeadersAccess(int32_t process_id, const GURL& resource_url) const;

  net::NetworkQualityEstimator* network_quality_estimator() {
    return network_quality_estimator_manager_->GetNetworkQualityEstimator();
  }
  net::NetLog* net_log() const;
  KeepaliveStatisticsRecorder* keepalive_statistics_recorder() {
    return &keepalive_statistics_recorder_;
  }
  net::HostResolverManager* host_resolver_manager() {
    return host_resolver_manager_.get();
  }
  net::HostResolver::Factory* host_resolver_factory() {
    return host_resolver_factory_.get();
  }
  HttpAuthCacheCopier* http_auth_cache_copier() {
    return http_auth_cache_copier_.get();
  }

  FirstPartySetsManager* first_party_sets_manager() const {
    return first_party_sets_manager_.get();
  }

  network::tpcd::metadata::Manager* tpcd_metadata_manager() const {
    return tpcd_metadata_manager_.get();
  }

  ip_protection::MaskedDomainListManager* masked_domain_list_manager() const {
    return masked_domain_list_manager_.get();
  }

  void set_host_resolver_factory_for_testing(
      std::unique_ptr<net::HostResolver::Factory> host_resolver_factory) {
    host_resolver_factory_ = std::move(host_resolver_factory);
  }

  bool split_auth_cache_by_network_isolation_key() const {
    return split_auth_cache_by_network_isolation_key_;
  }

  // From initialization on, this will be non-null and will always point to the
  // same object (although the object's state can change on updates to the
  // commitments). As a consequence, it's safe to store long-lived copies of the
  // pointer.
  const TrustTokenKeyCommitments* trust_token_key_commitments() const {
    return trust_token_key_commitments_.get();
  }

#if BUILDFLAG(IS_CT_SUPPORTED)
  SCTAuditingCache* sct_auditing_cache() { return sct_auditing_cache_.get(); }

  const std::vector<mojom::CTLogInfoPtr>& log_list() const { return log_list_; }

  bool is_ct_enforcement_enabled_for_testing() const {
    return ct_enforcement_enabled_;
  }
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  bool pins_list_updated() const { return pins_list_updated_; }

  const std::vector<net::TransportSecurityState::PinSet>& pinsets() const {
    return pinsets_;
  }

  const std::vector<net::TransportSecurityState::PinSetInfo>& host_pins()
      const {
    return host_pins_;
  }

  base::Time pins_list_update_time() const { return pins_list_update_time_; }

  bool data_use_updates_enabled() const { return data_use_updates_enabled_; }

  const mojom::HttpAuthDynamicParamsPtr&
  http_auth_dynamic_network_service_params_for_testing() const {
    return http_auth_dynamic_network_service_params_;
  }

  mojom::URLLoaderNetworkServiceObserver*
  GetDefaultURLLoaderNetworkServiceObserver();

  RestrictedCookieManager::UmaMetricsUpdater* metrics_updater() const {
    return metrics_updater_.get();
  }

  static NetworkService* GetNetworkServiceForTesting();

 private:
  class DelayedDohProbeActivator;

  void InitMockNetworkChangeNotifierForTesting();

  void DestroyNetworkContexts();

  // Called by a NetworkContext when its mojo pipe is closed. Deletes the
  // context.
  void OnNetworkContextConnectionClosed(NetworkContext* network_context);

  // Sets First-Party Set data after having read it from a file.
  void OnReadFirstPartySetsFile(const std::string& raw_sets);

  void SetSystemDnsResolver(
      mojo::PendingRemote<mojom::SystemDnsResolver> override_remote);

  void SetEnvironment(std::vector<mojom::EnvironmentVariablePtr> environment);

  bool initialized_ = false;

  enum class FunctionTag : uint8_t {
    None,
    ConfigureStubHostResolver,
    SetTestDohConfigForTesting,
  };

  mojo::Remote<network::mojom::NetworkAnnotationMonitor>
      network_annotation_monitor_;

  std::unique_ptr<RestrictedCookieManager::UmaMetricsUpdater> metrics_updater_;

  FunctionTag dns_config_overrides_set_by_ = FunctionTag::None;

  raw_ptr<net::NetLog> net_log_;

  std::unique_ptr<NetLogProxySink> net_log_proxy_sink_;

  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  // When capturing NetLog events, this keeps a NetworkContext's polled data
  // on the destruction of the NetworkContext.
  base::Value::List net_log_polled_data_list_;

  net::TraceNetLogObserver trace_net_log_observer_;

  KeepaliveStatisticsRecorder keepalive_statistics_recorder_;

  std::unique_ptr<NetworkChangeManager> network_change_manager_;

  // Observer that logs network changes to the NetLog. Must be below the NetLog
  // and the NetworkChangeNotifier (Once this class creates it), so it's
  // destroyed before them. Must be below the |network_change_manager_|, which
  // it references.
  std::unique_ptr<net::LoggingNetworkChangeObserver> network_change_observer_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  // Globally-scoped state for First-Party Sets.
  std::unique_ptr<FirstPartySetsManager> first_party_sets_manager_;

  mojo::Receiver<mojom::NetworkService> receiver_{this};

  mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      default_url_loader_network_service_observer_;

  std::unique_ptr<NetworkQualityEstimatorManager>
      network_quality_estimator_manager_;

  std::unique_ptr<DnsConfigChangeManager> dns_config_change_manager_;

  std::unique_ptr<net::HostResolverManager> host_resolver_manager_;
  std::unique_ptr<net::HostResolver::Factory> host_resolver_factory_;
  std::unique_ptr<HttpAuthCacheCopier> http_auth_cache_copier_;

  // Members that would store the http auth network_service related params.
  // These Params are later used by NetworkContext to create
  // HttpAuthPreferences.
  mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_network_service_params_;
  mojom::HttpAuthStaticParamsPtr http_auth_static_network_service_params_;

  // NetworkContexts created by CreateNetworkContext(). They call into the
  // NetworkService when their connection is closed so that it can delete
  // them.  It will also delete them when the NetworkService itself is torn
  // down, as NetworkContexts share global state owned by the NetworkService, so
  // must be destroyed first.
  //
  // NetworkContexts created by CreateNetworkContextWithBuilder() are not owned
  // by the NetworkService, and must be destroyed by their owners before the
  // NetworkService itself is.
  std::set<std::unique_ptr<NetworkContext>, base::UniquePtrComparator>
      owned_network_contexts_;

  // List of all NetworkContexts that are associated with the NetworkService,
  // including ones it does not own.
  // TODO(mmenke): Once the NetworkService always owns NetworkContexts, merge
  // this with |owned_network_contexts_|.
  std::set<raw_ptr<NetworkContext, SetExperimental>> network_contexts_;

  std::unique_ptr<ip_protection::MaskedDomainListManager>
      masked_domain_list_manager_;

  // A per-process_id map of origins that are white-listed to allow
  // them to request raw headers for resources they request.
  std::map<int32_t, base::flat_set<url::Origin>>
      raw_headers_access_origins_by_pid_;

  bool quic_disabled_ = false;

  // Whether new NetworkContexts will be configured to partition their
  // HttpAuthCaches by NetworkIsolationKey.
  bool split_auth_cache_by_network_isolation_key_ = false;

  // Globally-scoped cryptographic state for the Trust Tokens protocol
  // (https://github.com/wicg/trust-token-api), updated via a Mojo IPC and
  // provided to NetworkContexts via the getter.
  std::unique_ptr<TrustTokenKeyCommitments> trust_token_key_commitments_;

  std::unique_ptr<DelayedDohProbeActivator> doh_probe_activator_;

#if BUILDFLAG(IS_CT_SUPPORTED)
  std::unique_ptr<SCTAuditingCache> sct_auditing_cache_;

  std::vector<mojom::CTLogInfoPtr> log_list_;

  bool ct_enforcement_enabled_ = true;
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  bool pins_list_updated_ = false;

  std::vector<net::TransportSecurityState::PinSet> pinsets_;

  std::vector<net::TransportSecurityState::PinSetInfo> host_pins_;

  base::Time pins_list_update_time_;

  bool data_use_updates_enabled_ = false;

  // This is used only in tests. It avoids leaky SystemDnsConfigChangeNotifiers
  // leaking stale listeners between tests.
  std::unique_ptr<net::NetworkChangeNotifier> mock_network_change_notifier_;

#if BUILDFLAG(IS_LINUX)
  mojo::Remote<mojom::GssapiLibraryLoadObserver> gssapi_library_load_observer_;
#endif  // BUILDFLAG(IS_LINUX)

  std::unique_ptr<network::tpcd::metadata::Manager> tpcd_metadata_manager_;

  base::WeakPtrFactory<NetworkService> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_H_
