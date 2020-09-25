// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/log/net_log.h"
#include "net/log/trace_net_log_observer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/first_party_sets/preloaded_first_party_sets.h"
#include "services/network/keepalive_statistics_recorder.h"
#include "services/network/network_change_manager.h"
#include "services/network/network_quality_estimator_manager.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_quality_estimator_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace net {
class FileNetLogObserver;
class HostResolverManager;
class HttpAuthHandlerFactory;
class LoggingNetworkChangeObserver;
class NetworkQualityEstimator;
class URLRequestContext;
}  // namespace net

namespace network {

class CRLSetDistributor;
class DnsConfigChangeManager;
class HttpAuthCacheCopier;
class LegacyTLSConfigDistributor;
class NetLogProxySink;
class NetworkContext;
class NetworkService;
class NetworkUsageAccumulator;
class SCTAuditingCache;

// DataPipeUseTracker tracks the mojo data pipe usage in the network
// service.
class COMPONENT_EXPORT(NETWORK_SERVICE) DataPipeUseTracker final {
 public:
  enum DataPipeUser {
    kUrlLoader = 0,
    kWebSocket = 1,
  };
  // |network_service| must outlive |this|.
  DataPipeUseTracker(NetworkService* network_service, DataPipeUser user);
  DataPipeUseTracker(DataPipeUseTracker&&);
  ~DataPipeUseTracker();
  DataPipeUseTracker(const DataPipeUseTracker&) = delete;
  DataPipeUseTracker& operator=(const DataPipeUseTracker&) = delete;

  // Call this when the associated data pipe is created.
  void Activate();
  // Call this when (one end of) the associated data pipe is dropped.
  void Reset();

 private:
  enum State {
    kInit,
    kActivated,
    kReset,
  };
  NetworkService* const network_service_;
  const DataPipeUser user_;

  State state_ = State::kInit;
};

using DataPipeUser = DataPipeUseTracker::DataPipeUser;

class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkService
    : public mojom::NetworkService {
 public:
  static const base::TimeDelta kInitialDohProbeTimeout;

  NetworkService(std::unique_ptr<service_manager::BinderRegistry> registry,
                 mojo::PendingReceiver<mojom::NetworkService> receiver =
                     mojo::NullReceiver(),
                 bool delay_initialization_until_set_client = false);

  ~NetworkService() override;

  // Call to inform the NetworkService that OSCrypt::SetConfig() has already
  // been invoked, so OSCrypt::SetConfig() does not need to be called before
  // encrypted storage can be used.
  void set_os_crypt_is_configured();

  // Allows late binding if the mojo receiver wasn't specified in the
  // constructor.
  void Bind(mojo::PendingReceiver<mojom::NetworkService> receiver);

  // Allows the browser process to synchronously initialize the NetworkService.
  // TODO(jam): remove this once the old path is gone.
  void Initialize(mojom::NetworkServiceParamsPtr params,
                  bool mock_network_change_notifier = false);

  // Creates a NetworkService instance on the current thread.
  static std::unique_ptr<NetworkService> Create(
      mojo::PendingReceiver<mojom::NetworkService> receiver);

  // Creates a testing instance of NetworkService not bound to an actual
  // Service pipe. This instance must be driven by direct calls onto the
  // NetworkService object.
  static std::unique_ptr<NetworkService> CreateForTesting();

  // These are called by NetworkContexts as they are being created and
  // destroyed.
  // TODO(mmenke):  Remove once all NetworkContexts are owned by the
  // NetworkService.
  void RegisterNetworkContext(NetworkContext* network_context);
  void DeregisterNetworkContext(NetworkContext* network_context);

  // Invokes net::CreateNetLogEntriesForActiveObjects(observer) on all
  // URLRequestContext's known to |this|.
  void CreateNetLogEntriesForActiveObjects(
      net::NetLog::ThreadSafeObserver* observer);

  // mojom::NetworkService implementation:
  void SetClient(mojo::PendingRemote<mojom::NetworkServiceClient> client,
                 mojom::NetworkServiceParamsPtr params) override;
#if defined(OS_CHROMEOS)
  void ReinitializeLogging(mojom::LoggingSettingsPtr settings) override;
#endif
  void StartNetLog(base::File file,
                   net::NetLogCaptureMode capture_mode,
                   base::Value constants) override;
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
      base::Optional<std::vector<mojom::DnsOverHttpsServerPtr>>
          dns_over_https_servers) override;
  void DisableQuic() override;
  void SetUpHttpAuth(
      mojom::HttpAuthStaticParamsPtr http_auth_static_params) override;
  void ConfigureHttpAuthPrefs(
      mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) override;
  void SetRawHeadersAccess(int32_t process_id,
                           const std::vector<url::Origin>& origins) override;
  void SetMaxConnectionsPerProxy(int32_t max_connections) override;
  void GetNetworkChangeManager(
      mojo::PendingReceiver<mojom::NetworkChangeManager> receiver) override;
  void GetNetworkQualityEstimatorManager(
      mojo::PendingReceiver<mojom::NetworkQualityEstimatorManager> receiver)
      override;
  void GetDnsConfigChangeManager(
      mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver) override;
  void GetTotalNetworkUsages(
      mojom::NetworkService::GetTotalNetworkUsagesCallback callback) override;
  void GetNetworkList(
      uint32_t policy,
      mojom::NetworkService::GetNetworkListCallback callback) override;
  void UpdateCRLSet(
      base::span<const uint8_t> crl_set,
      mojom::NetworkService::UpdateCRLSetCallback callback) override;
  void UpdateLegacyTLSConfig(
      base::span<const uint8_t> config,
      mojom::NetworkService::UpdateLegacyTLSConfigCallback callback) override;
  void OnCertDBChanged() override;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void SetCryptConfig(mojom::CryptConfigPtr crypt_config) override;
#endif
#if defined(OS_WIN) || defined(OS_MAC)
  void SetEncryptionKey(const std::string& encryption_key) override;
#endif
  void AddCorbExceptionForPlugin(int32_t process_id) override;
  void AddAllowedRequestInitiatorForPlugin(
      int32_t process_id,
      const url::Origin& allowed_request_initiator) override;
  void RemoveSecurityExceptionsForPlugin(int32_t process_id) override;
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel
                            memory_pressure_level) override;
  void OnPeerToPeerConnectionsCountChange(uint32_t count) override;
#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state) override;
#endif
  void SetEnvironment(
      std::vector<mojom::EnvironmentVariablePtr> environment) override;
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   base::OnceClosure done) override;
#if BUILDFLAG(IS_CT_SUPPORTED)
  void ClearSCTAuditingCache() override;
  void ConfigureSCTAuditing(
      bool enabled,
      double sampling_rate,
      const GURL& reporting_uri,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<mojom::URLLoaderFactory> factory) override;
#endif

#if defined(OS_ANDROID)
  void DumpWithoutCrashing(base::Time dump_request_time) override;
#endif
  void BindTestInterface(
      mojo::PendingReceiver<mojom::NetworkServiceTest> receiver) override;
  void SetPreloadedFirstPartySets(const std::string& raw_sets) override;

  // Returns an HttpAuthHandlerFactory for the given NetworkContext.
  std::unique_ptr<net::HttpAuthHandlerFactory> CreateHttpAuthHandlerFactory(
      NetworkContext* network_context);

  // Notification that a URLLoader is about to start.
  void OnBeforeURLRequest();

  bool quic_disabled() const { return quic_disabled_; }
  bool HasRawHeadersAccess(int32_t process_id, const GURL& resource_url) const;

  bool IsInitiatorAllowedForPlugin(int process_id,
                                   const url::Origin& request_initiator);

  mojom::NetworkServiceClient* client() {
    return client_.is_bound() ? client_.get() : nullptr;
  }
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
  NetworkUsageAccumulator* network_usage_accumulator() {
    return network_usage_accumulator_.get();
  }
  HttpAuthCacheCopier* http_auth_cache_copier() {
    return http_auth_cache_copier_.get();
  }

  CRLSetDistributor* crl_set_distributor() {
    return crl_set_distributor_.get();
  }

  LegacyTLSConfigDistributor* legacy_tls_config_distributor() {
    return legacy_tls_config_distributor_.get();
  }

  bool os_crypt_config_set() const { return os_crypt_config_set_; }

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
#endif

  void OnDataPipeCreated(DataPipeUser user);
  void OnDataPipeDropped(DataPipeUser user);
  void StopMetricsTimerForTesting();

  static NetworkService* GetNetworkServiceForTesting();

 private:
  class DelayedDohProbeActivator;

  void DestroyNetworkContexts();

  // Called by a NetworkContext when its mojo pipe is closed. Deletes the
  // context.
  void OnNetworkContextConnectionClosed(NetworkContext* network_context);

  // Starts the timer to call NetworkServiceClient::OnLoadingStateUpdate(), if
  // timer isn't already running, |waiting_on_load_state_ack_| is false, and
  // there are live URLLoaders.
  // Only works when network service is enabled.
  void MaybeStartUpdateLoadInfoTimer();

  // Checks all pending requests and updates the load info if necessary.
  void UpdateLoadInfo();

  // Invoked once the browser has acknowledged receiving the previous LoadInfo.
  // Starts timer call UpdateLoadInfo() again, if needed.
  void AckUpdateLoadInfo();

  void ReportMetrics();

  bool initialized_ = false;

  net::NetLog* net_log_;

  std::unique_ptr<NetLogProxySink> net_log_proxy_sink_;

  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  net::TraceNetLogObserver trace_net_log_observer_;

  mojo::Remote<mojom::NetworkServiceClient> client_;

  KeepaliveStatisticsRecorder keepalive_statistics_recorder_;

  std::unique_ptr<NetworkChangeManager> network_change_manager_;

  // Observer that logs network changes to the NetLog. Must be below the NetLog
  // and the NetworkChangeNotifier (Once this class creates it), so it's
  // destroyed before them. Must be below the |network_change_manager_|, which
  // it references.
  std::unique_ptr<net::LoggingNetworkChangeObserver> network_change_observer_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  mojo::Receiver<mojom::NetworkService> receiver_{this};

  std::unique_ptr<NetworkQualityEstimatorManager>
      network_quality_estimator_manager_;

  std::unique_ptr<DnsConfigChangeManager> dns_config_change_manager_;

  std::unique_ptr<net::HostResolverManager> host_resolver_manager_;
  std::unique_ptr<net::HostResolver::Factory> host_resolver_factory_;
  std::unique_ptr<NetworkUsageAccumulator> network_usage_accumulator_;
  std::unique_ptr<HttpAuthCacheCopier> http_auth_cache_copier_;

  // Members that would store the http auth network_service related params.
  // These Params are later used by NetworkContext to create
  // HttpAuthPreferences.
  mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_network_service_params_;
  mojom::HttpAuthStaticParamsPtr http_auth_static_network_service_params_;

  // Globally-scoped state for First-Party Sets that were preloaded (and
  // updated) via the component updater.
  std::unique_ptr<PreloadedFirstPartySets> preloaded_first_party_sets_;

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
  std::set<NetworkContext*> network_contexts_;

  // A per-process_id map of origins that are white-listed to allow
  // them to request raw headers for resources they request.
  std::map<int32_t, base::flat_set<url::Origin>>
      raw_headers_access_origins_by_pid_;

  bool quic_disabled_ = false;

  bool os_crypt_config_set_ = false;

  std::unique_ptr<CRLSetDistributor> crl_set_distributor_;

  std::unique_ptr<LegacyTLSConfigDistributor> legacy_tls_config_distributor_;

  // A timer that periodically calls UpdateLoadInfo while there are pending
  // loads and not waiting on an ACK from the client for the last sent
  // LoadInfo callback.
  base::OneShotTimer update_load_info_timer_;
  // True if a LoadInfoList has been sent to the client, but has yet to be
  // acknowledged.
  bool waiting_on_load_state_ack_ = false;

  // A timer that periodically calls ReportMetrics every 20 minutes.
  base::RepeatingTimer metrics_trigger_timer_;

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
#endif

  // Map from a renderer process id, to the set of plugin origins embedded by
  // that renderer process (the renderer will proxy requests from PPAPI - such
  // requests should have their initiator origin within the set stored here).
  std::map<int, std::set<url::Origin>> plugin_origins_;

  struct DataPipeUsage final {
    int current = 0;
    int max = 0;
    int min = 0;
  };
  base::flat_map<DataPipeUser, DataPipeUsage> data_pipe_use_;

  DISALLOW_COPY_AND_ASSIGN(NetworkService);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_H_
