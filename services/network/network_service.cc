// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_passive.h"
#include "net/base/port_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/signed_tree_head.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "net/dns/test_dns_config_service.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/url_request/url_request_context.h"
#include "services/network/dns_config_change_manager.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/http_auth_cache_copier.h"
#include "services/network/net_log_exporter.h"
#include "services/network/net_log_proxy_sink.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/crash_keys.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/load_info_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/mojom/key_pinning.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/system_dns_resolution.mojom-forward.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/tpcd/metadata/manager.h"
#include "services/network/url_loader.h"

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)

#include "components/os_crypt/sync/key_storage_config_linux.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "services/network/network_change_notifier_passive_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#include "net/android/http_auth_negotiate_android.h"
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/sct_auditing/sct_auditing_cache.h"
#endif

namespace net {
class FirstPartySetEntry;
}

namespace network {

namespace {

NetworkService* g_network_service = nullptr;

std::unique_ptr<net::NetworkChangeNotifier> CreateNetworkChangeNotifierIfNeeded(
    net::NetworkChangeNotifier::ConnectionType initial_connection_type,
    net::NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype,
    bool mock_network_change_notifier) {
  // There is a global singleton net::NetworkChangeNotifier if NetworkService
  // is running inside of the browser process.
  if (mock_network_change_notifier) {
    return net::NetworkChangeNotifier::CreateMockIfNeeded();
  }
  return net::NetworkChangeNotifier::CreateIfNeeded(initial_connection_type,
                                                    initial_connection_subtype);
}

void OnGetNetworkList(std::unique_ptr<net::NetworkInterfaceList> networks,
                      mojom::NetworkService::GetNetworkListCallback callback,
                      bool success) {
  if (success) {
    std::move(callback).Run(*networks);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_KERBEROS)
// Used for Negotiate authentication on Android, which needs to generate tokens
// in the browser process.
class NetworkServiceAuthNegotiateAndroid : public net::HttpAuthMechanism {
 public:
  NetworkServiceAuthNegotiateAndroid(NetworkContext* network_context,
                                     const net::HttpAuthPreferences* prefs)
      : network_context_(network_context), auth_negotiate_(prefs) {}
  ~NetworkServiceAuthNegotiateAndroid() override = default;

  // HttpAuthMechanism implementation:
  bool Init(const net::NetLogWithSource& net_log) override {
    return auth_negotiate_.Init(net_log);
  }

  bool NeedsIdentity() const override {
    return auth_negotiate_.NeedsIdentity();
  }

  bool AllowsExplicitCredentials() const override {
    return auth_negotiate_.AllowsExplicitCredentials();
  }

  net::HttpAuth::AuthorizationResult ParseChallenge(
      net::HttpAuthChallengeTokenizer* tok) override {
    return auth_negotiate_.ParseChallenge(tok);
  }

  int GenerateAuthToken(const net::AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const net::NetLogWithSource& net_log,
                        net::CompletionOnceCallback callback) override {
    network_context_->client()->OnGenerateHttpNegotiateAuthToken(
        auth_negotiate_.server_auth_token(), auth_negotiate_.can_delegate(),
        auth_negotiate_.GetAuthAndroidNegotiateAccountType(), spn,
        base::BindOnce(&NetworkServiceAuthNegotiateAndroid::Finish,
                       weak_factory_.GetWeakPtr(), auth_token,
                       std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  void SetDelegation(net::HttpAuth::DelegationType delegation_type) override {
    auth_negotiate_.SetDelegation(delegation_type);
  }

 private:
  void Finish(std::string* auth_token_out,
              net::CompletionOnceCallback callback,
              int result,
              const std::string& auth_token) {
    *auth_token_out = auth_token;
    std::move(callback).Run(result);
  }

  raw_ptr<NetworkContext> network_context_ = nullptr;
  net::android::HttpAuthNegotiateAndroid auth_negotiate_;
  base::WeakPtrFactory<NetworkServiceAuthNegotiateAndroid> weak_factory_{this};
};

std::unique_ptr<net::HttpAuthMechanism> CreateAuthSystem(
    NetworkContext* network_context,
    const net::HttpAuthPreferences* prefs) {
  return std::make_unique<NetworkServiceAuthNegotiateAndroid>(network_context,
                                                              prefs);
}
#endif

// Called when NetworkService received a bad IPC message (but only when
// NetworkService is running in a separate process - otherwise the existing bad
// message handling inside the Browser process is sufficient).
void HandleBadMessage(const std::string& error) {
  LOG(WARNING) << "Mojo error in NetworkService: " << error;
  mojo::debug::ScopedMessageErrorCrashKey crash_key_value(error);
  base::debug::DumpWithoutCrashing();
  network::debug::ClearDeserializationCrashKeyString();
}

// Runs `results_cb` on `sequenced_task_runner` with an empty result and
// net::ERR_ABORTED.
void AsyncResolveSystemDnsWithEmptyResult(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    net::SystemDnsResultsCallback results_cb) {
  sequenced_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(results_cb), net::AddressList(), 0,
                                net::ERR_ABORTED));
}

void ResolveSystemDnsWithMojo(
    const mojo::Remote<mojom::SystemDnsResolver>& system_dns_override,
    const std::optional<std::string>& hostname,
    net::AddressFamily addr_family,
    net::HostResolverFlags flags,
    net::SystemDnsResultsCallback results_cb,
    net::handles::NetworkHandle network) {
  std::pair<net::SystemDnsResultsCallback, net::SystemDnsResultsCallback>
      duplicated_results_cbs = base::SplitOnceCallback(std::move(results_cb));
  // In the case that the callback is dropped without ever being run (if
  // `system_dns_override` disconnects), `results_cb` should run asynchronously
  // with an empty result. `results_cb` should never be run synchronously.
  base::OnceClosure drop_handler =
      base::BindOnce(&AsyncResolveSystemDnsWithEmptyResult,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(duplicated_results_cbs.second));
  auto results_cb_with_default_invoke = mojo::WrapCallbackWithDropHandler(
      std::move(duplicated_results_cbs.first), std::move(drop_handler));
  system_dns_override->Resolve(hostname, addr_family, flags, network,
                               std::move(results_cb_with_default_invoke));
}

// Creating an instance of this class starts exporting UMA data related to
// RestrictedCookieManager. There should only be one instance of this class at a
// time and it should be kept around for the duration of the program. This is
// accomplished by having NetworkService own the instance.
class RestrictedCookieManagerMetrics
    : public RestrictedCookieManager::UmaMetricsUpdater {
 public:
  RestrictedCookieManagerMetrics() {
    histogram_ = base::Histogram::FactoryGet(
        "Net.RestrictedCookieManager.GetCookiesString.Count30Seconds", 1, 10000,
        50, base::HistogramBase::kUmaTargetedHistogramFlag);
    timer_.Start(
        FROM_HERE, base::Seconds(30),
        base::BindRepeating(&RestrictedCookieManagerMetrics::OnTimerTick,
                            base::Unretained(this)));
  }
  ~RestrictedCookieManagerMetrics() override = default;

 private:
  void OnGetCookiesString() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ++get_cookies_string_count_;
  }

  void OnTimerTick() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    histogram_->Add(get_cookies_string_count_);
    get_cookies_string_count_ = 0;
  }

  SEQUENCE_CHECKER(sequence_checker_);
  uint64_t get_cookies_string_count_{0};
  raw_ptr<base::HistogramBase> histogram_;
  base::RepeatingTimer timer_;
};

}  // namespace

// static
const base::TimeDelta NetworkService::kInitialDohProbeTimeout =
    base::Seconds(5);

// Handler of delaying calls to NetworkContext::ActivateDohProbes() until after
// an initial service startup delay.
class NetworkService::DelayedDohProbeActivator {
 public:
  explicit DelayedDohProbeActivator(NetworkService* network_service)
      : network_service_(network_service) {
    DCHECK(network_service_);

    // Delay initial DoH probes to prevent interference with startup tasks.
    doh_probes_timer_.Start(
        FROM_HERE, NetworkService::kInitialDohProbeTimeout,
        base::BindOnce(&DelayedDohProbeActivator::ActivateAllDohProbes,
                       base::Unretained(this)));
  }

  DelayedDohProbeActivator(const DelayedDohProbeActivator&) = delete;
  DelayedDohProbeActivator& operator=(const DelayedDohProbeActivator&) = delete;

  // Activates DoH probes for |network_context| iff the initial startup delay
  // has expired. Intended to be called on registration of contexts to activate
  // probes for contexts created and registered after the initial delay has
  // expired.
  void MaybeActivateDohProbes(NetworkContext* network_context) {
    // If timer is still running, probes will be started on completion.
    if (doh_probes_timer_.IsRunning()) {
      return;
    }

    network_context->ActivateDohProbes();
  }

  // Attempts to activate DoH probes for all contexts registered with the
  // service. Intended to be called on expiration of |doh_probes_timer_| to
  // activate probes for contexts registered during the initial delay.
  void ActivateAllDohProbes() {
    for (NetworkContext* network_context :
         network_service_->network_contexts_) {
      MaybeActivateDohProbes(network_context);
    }
  }

 private:
  const raw_ptr<NetworkService> network_service_;

  // If running, DoH probes will be started on completion. If not running, DoH
  // probes may be started at any time.
  base::OneShotTimer doh_probes_timer_;
};

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry,
    mojo::PendingReceiver<mojom::NetworkService> receiver,
    bool delay_initialization_until_set_client)
    : net_log_(net::NetLog::Get()), registry_(std::move(registry)) {
  DCHECK(!g_network_service);
  g_network_service = this;

  // |registry_| is nullptr when a NetworkService is out-of-process.
  if (registry_) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(&HandleBadMessage));
#if BUILDFLAG(IS_LINUX)
    if (base::FeatureList::IsEnabled(
            net::features::kAddressTrackerLinuxIsProxied)) {
      net::NetworkChangeNotifier::SetFactory(
          new network::NetworkChangeNotifierPassiveFactory());
    }
#endif
  }

  if (receiver.is_valid()) {
    Bind(std::move(receiver));
  }

  if (!delay_initialization_until_set_client) {
    Initialize(mojom::NetworkServiceParams::New());
  }
}

void NetworkService::Initialize(mojom::NetworkServiceParamsPtr params,
                                bool mock_network_change_notifier) {
  if (initialized_) {
    return;
  }

  initialized_ = true;

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // Measure Android kernels with missing AT_HWCAP2 auxv fields. See
  // https://crbug.com/boringssl/46.
  UMA_HISTOGRAM_BOOLEAN("Net.NeedsHWCAP2Workaround",
                        CRYPTO_needs_hwcap2_workaround());
#endif

  if (!params->environment.empty()) {
    SetEnvironment(std::move(params->environment));
  }

  if (params->system_dns_resolver) {
    SetSystemDnsResolver(std::move(params->system_dns_resolver));
  }

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier =
      CreateNetworkChangeNotifierIfNeeded(
          net::NetworkChangeNotifier::ConnectionType(
              params->initial_connection_type),
          net::NetworkChangeNotifier::ConnectionSubtype(
              params->initial_connection_subtype),
          mock_network_change_notifier);

#if BUILDFLAG(IS_LINUX)
  if (params->initial_address_map) {
    // The NetworkChangeNotifierPassive should only be included if it's
    // necessary to instantiate an AddressMapCacheLinux rather than an
    // AddressTrackerLinux.
    DCHECK(base::FeatureList::IsEnabled(
        net::features::kAddressTrackerLinuxIsProxied));
    // There should be a factory that creates NetworkChangeNotifierPassives.
    DCHECK(net::NetworkChangeNotifier::GetFactory());
    // Network service should be out of process or it's unsandboxed and can just
    // use AddressTrackerLinux.
    DCHECK(registry_);
    network_change_notifier->GetAddressMapOwner()
        ->GetAddressMapCacheLinux()
        ->SetCachedInfo(std::move(params->initial_address_map->address_map),
                        std::move(params->initial_address_map->online_links));
  }
#endif  // BUILDFLAG(IS_LINUX)

  network_change_manager_ = std::make_unique<NetworkChangeManager>(
      std::move(network_change_notifier));

  trace_net_log_observer_.WatchForTraceStart(net_log_);

  // Add an observer that will emit network change events to |net_log_|.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_ =
      std::make_unique<net::LoggingNetworkChangeObserver>(net_log_);

  network_quality_estimator_manager_ =
      std::make_unique<NetworkQualityEstimatorManager>(net_log_);

  dns_config_change_manager_ = std::make_unique<DnsConfigChangeManager>();

  host_resolver_manager_ = std::make_unique<net::HostResolverManager>(
      net::HostResolver::ManagerOptions(),
      net::NetworkChangeNotifier::GetSystemDnsConfigNotifier(), net_log_);
  host_resolver_factory_ = std::make_unique<net::HostResolver::Factory>();

  http_auth_cache_copier_ = std::make_unique<HttpAuthCacheCopier>();

  doh_probe_activator_ = std::make_unique<DelayedDohProbeActivator>(this);

  trust_token_key_commitments_ = std::make_unique<TrustTokenKeyCommitments>();

  if (params->default_observer) {
    default_url_loader_network_service_observer_.Bind(
        std::move(params->default_observer));
  }

  first_party_sets_manager_ =
      std::make_unique<FirstPartySetsManager>(params->first_party_sets_enabled);

  tpcd_metadata_manager_ = std::make_unique<network::tpcd::metadata::Manager>();

  masked_domain_list_manager_ =
      std::make_unique<ip_protection::MaskedDomainListManager>(
          params->ip_protection_proxy_bypass_policy);

#if BUILDFLAG(IS_CT_SUPPORTED)
  constexpr size_t kMaxSCTAuditingCacheEntries = 1024;
  sct_auditing_cache_ =
      std::make_unique<SCTAuditingCache>(kMaxSCTAuditingCacheEntries);
#endif

  if (base::FeatureList::IsEnabled(features::kGetCookiesStringUma)) {
    metrics_updater_ = std::make_unique<RestrictedCookieManagerMetrics>();
  }
}

NetworkService::~NetworkService() {
  DCHECK_EQ(this, g_network_service);

  doh_probe_activator_.reset();

  g_network_service = nullptr;
  // Destroy owned network contexts.
  DestroyNetworkContexts();

  // All NetworkContexts (Owned and unowned) must have been deleted by this
  // point.
  DCHECK(network_contexts_.empty());

  if (file_net_log_observer_) {
    auto polled_data =
        std::make_unique<base::Value>(std::move(net_log_polled_data_list_));
    file_net_log_observer_->StopObserving(std::move(polled_data),
                                          base::OnceClosure());
  }

  if (initialized_) {
    trace_net_log_observer_.StopWatchForTraceStart();
  }

  net::SetSystemDnsResolverOverride(base::NullCallback());
}

void NetworkService::ReplaceSystemDnsConfigForTesting(
    base::OnceClosure replace_cb) {
  // Create a test `net::DnsConfigService` that will yield a dummy config once.
  auto config_service = std::make_unique<net::TestDnsConfigService>();
  config_service->SetConfigForRefresh(
      net::DnsConfig({net::IPEndPoint(net::IPAddress::IPv4Localhost(), 1234)}));

  // Replace the existing `net::DnsConfigService` and flush the lines once to
  // replace the system DNS config, in case we already received it.
  auto* notifier = net::NetworkChangeNotifier::GetSystemDnsConfigNotifier();
  DCHECK(notifier);
  notifier->SetDnsConfigServiceForTesting(  // IN-TEST
      std::move(config_service), std::move(replace_cb));

  // Force-disable the system resolver so that HostResolverManager will actually
  // use the replacement config.
  host_resolver_manager_->DisableSystemResolverForTesting();  // IN-TEST
}

void NetworkService::SetNetworkAnnotationMonitor(
    mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor> remote) {
  network_annotation_monitor_.Bind(std::move(remote));
}

void NetworkService::NotifyNetworkRequestWithAnnotation(
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  if (network_annotation_monitor_.is_bound()) {
    network_annotation_monitor_->Report(traffic_annotation.unique_id_hash_code);
  }
}

void NetworkService::SetTestDohConfigForTesting(
    net::SecureDnsMode secure_dns_mode,
    const net::DnsOverHttpsConfig& doh_config) {
  DCHECK_EQ(dns_config_overrides_set_by_, FunctionTag::None);
  dns_config_overrides_set_by_ = FunctionTag::SetTestDohConfigForTesting;

  // Overlay DoH settings on top of the system config, whenever it is received.
  net::DnsConfigOverrides overrides;
  overrides.secure_dns_mode = secure_dns_mode;
  overrides.dns_over_https_config = doh_config;
  host_resolver_manager_->SetDnsConfigOverrides(std::move(overrides));

  // Force-disable the system resolver so that HostResolverManager will actually
  // query the test DoH server.
  host_resolver_manager_->DisableSystemResolverForTesting();  // IN-TEST
}

std::unique_ptr<NetworkService> NetworkService::Create(
    mojo::PendingReceiver<mojom::NetworkService> receiver) {
  return std::make_unique<NetworkService>(nullptr, std::move(receiver));
}

// static
std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  auto network_service =
      std::make_unique<NetworkService>(nullptr /* binder_registry */);
  network_service->InitMockNetworkChangeNotifierForTesting();  // IN-TEST
  return network_service;
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
  if (quic_disabled_) {
    network_context->DisableQuic();
  }

  // The params may already be present, so we propagate it to this new
  // network_context. When params gets changed via ConfigureHttpAuthPrefs
  // method, we propagate the change to all NetworkContexts in
  // |network_contexts_|.
  if (http_auth_dynamic_network_service_params_) {
    network_context->OnHttpAuthDynamicParamsChanged(
        http_auth_dynamic_network_service_params_.get());
  }

  if (doh_probe_activator_) {
    doh_probe_activator_->MaybeActivateDohProbes(network_context);
  }

#if BUILDFLAG(IS_CT_SUPPORTED)
  network_context->url_request_context()
      ->transport_security_state()
      ->SetCTEmergencyDisabled(!ct_enforcement_enabled_);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkService::CreateNetLogEntriesForActiveObjects(
    net::NetLog::ThreadSafeObserver* observer) {
  std::set<net::URLRequestContext*> contexts;
  for (NetworkContext* nc : network_contexts_) {
    contexts.insert(nc->url_request_context());
  }
  return net::CreateNetLogEntriesForActiveObjects(contexts, observer);
}

void NetworkService::SetParams(mojom::NetworkServiceParamsPtr params) {
  Initialize(std::move(params));
}

void NetworkService::SetSystemDnsResolver(
    mojo::PendingRemote<mojom::SystemDnsResolver> override_remote) {
  CHECK(override_remote);

  // Using a Remote (as opposed to a SharedRemote) is fine as system host
  // resolver overrides should only be invoked on the main thread.
  mojo::Remote<mojom::SystemDnsResolver> system_dns_override(
      std::move(override_remote));

  // Note that if this override replaces a currently existing override, it wil
  // destruct the Remote<mojom::SystemDnsResolver> owned by the other override,
  // which will cancel all ongoing DNS resolutions.
  net::SetSystemDnsResolverOverride(base::BindRepeating(
      ResolveSystemDnsWithMojo, std::move(system_dns_override)));
}

void NetworkService::StartNetLog(base::File file,
                                 uint64_t max_total_size,
                                 net::NetLogCaptureMode capture_mode,
                                 base::Value::Dict constants) {
  if (max_total_size == net::FileNetLogObserver::kNoLimit) {
    StartNetLogUnbounded(std::move(file), capture_mode, std::move(constants));
  } else {
    StartNetLogBounded(std::move(file), max_total_size, capture_mode,
                       std::move(constants));
  }
}

void NetworkService::AttachNetLogProxy(
    mojo::PendingRemote<mojom::NetLogProxySource> proxy_source,
    mojo::PendingReceiver<mojom::NetLogProxySink> proxy_sink) {
  if (!net_log_proxy_sink_) {
    net_log_proxy_sink_ = std::make_unique<NetLogProxySink>();
  }
  net_log_proxy_sink_->AttachSource(std::move(proxy_source),
                                    std::move(proxy_sink));
}

void NetworkService::SetSSLKeyLogFile(base::File file) {
  net::SSLClientSocket::SetSSLKeyLogger(
      std::make_unique<net::SSLKeyLoggerImpl>(std::move(file)));
}

void NetworkService::CreateNetworkContext(
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    mojom::NetworkContextParamsPtr params) {
  owned_network_contexts_.emplace(std::make_unique<NetworkContext>(
      this, std::move(receiver), std::move(params),
      base::BindOnce(&NetworkService::OnNetworkContextConnectionClosed,
                     base::Unretained(this))));
}

void NetworkService::ConfigureStubHostResolver(
    bool insecure_dns_client_enabled,
    net::SecureDnsMode secure_dns_mode,
    const net::DnsOverHttpsConfig& dns_over_https_config,
    bool additional_dns_types_enabled) {
  // Enable or disable the insecure part of DnsClient. "DnsClient" is the class
  // that implements the stub resolver.
  host_resolver_manager_->SetInsecureDnsClientEnabled(
      insecure_dns_client_enabled, additional_dns_types_enabled);

  // Configure DNS over HTTPS.
  DCHECK(dns_config_overrides_set_by_ == FunctionTag::None ||
         dns_config_overrides_set_by_ ==
             FunctionTag::ConfigureStubHostResolver);
  dns_config_overrides_set_by_ = FunctionTag::ConfigureStubHostResolver;
  net::DnsConfigOverrides overrides;
  overrides.dns_over_https_config = dns_over_https_config;
  overrides.secure_dns_mode = secure_dns_mode;
  overrides.allow_dns_over_https_upgrade =
      base::FeatureList::IsEnabled(features::kDnsOverHttpsUpgrade);

  host_resolver_manager_->SetDnsConfigOverrides(overrides);
}

void NetworkService::DisableQuic() {
  quic_disabled_ = true;

  for (NetworkContext* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkService::SetUpHttpAuth(
    mojom::HttpAuthStaticParamsPtr http_auth_static_params) {
  DCHECK(!http_auth_static_network_service_params_);
  DCHECK(network_contexts_.empty());
  http_auth_static_network_service_params_ = std::move(http_auth_static_params);
}

void NetworkService::ConfigureHttpAuthPrefs(
    mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) {
  // We need to store it as a member variable because the method
  // NetworkService::RegisterNetworkContext(NetworkContext *network_context)
  // uses it to populate the HttpAuthPreferences of the incoming network_context
  // with the latest dynamic params of the NetworkService.
  http_auth_dynamic_network_service_params_ =
      std::move(http_auth_dynamic_params);

  for (NetworkContext* network_context : network_contexts_) {
    network_context->OnHttpAuthDynamicParamsChanged(
        http_auth_dynamic_network_service_params_.get());
  }
}

void NetworkService::SetRawHeadersAccess(
    int32_t process_id,
    const std::vector<url::Origin>& origins) {
  DCHECK(process_id);
  if (!origins.size()) {
    raw_headers_access_origins_by_pid_.erase(process_id);
  } else {
    raw_headers_access_origins_by_pid_[process_id] =
        base::flat_set<url::Origin>(origins.begin(), origins.end());
  }
}

void NetworkService::SetMaxConnectionsPerProxyChain(int32_t max_connections) {
  int new_limit = max_connections;
  if (new_limit < 0) {
    new_limit = net::kDefaultMaxSocketsPerProxyChain;
  }

  // Clamp the value between min_limit and max_limit.
  int max_limit = 99;
  int min_limit = net::ClientSocketPoolManager::max_sockets_per_group(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL);
  new_limit = std::clamp(new_limit, min_limit, max_limit);

  // Assign the global limit.
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_chain(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL, new_limit);
}

bool NetworkService::HasRawHeadersAccess(int32_t process_id,
                                         const GURL& resource_url) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id) {
    return true;
  }
  auto it = raw_headers_access_origins_by_pid_.find(process_id);
  if (it == raw_headers_access_origins_by_pid_.end()) {
    return false;
  }
  return it->second.find(url::Origin::Create(resource_url)) != it->second.end();
}

net::NetLog* NetworkService::net_log() const {
  return net_log_;
}

void NetworkService::GetNetworkChangeManager(
    mojo::PendingReceiver<mojom::NetworkChangeManager> receiver) {
  network_change_manager_->AddReceiver(std::move(receiver));
}

void NetworkService::GetNetworkQualityEstimatorManager(
    mojo::PendingReceiver<mojom::NetworkQualityEstimatorManager> receiver) {
  network_quality_estimator_manager_->AddReceiver(std::move(receiver));
}

void NetworkService::GetDnsConfigChangeManager(
    mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver) {
  dns_config_change_manager_->AddReceiver(std::move(receiver));
}

void NetworkService::GetNetworkList(
    uint32_t policy,
    mojom::NetworkService::GetNetworkListCallback callback) {
  auto networks = std::make_unique<net::NetworkInterfaceList>();
  auto* raw_networks = networks.get();
  // net::GetNetworkList may block depending on platform.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&net::GetNetworkList, raw_networks, policy),
      base::BindOnce(&OnGetNetworkList, std::move(networks),
                     std::move(callback)));
}

void NetworkService::OnTrustStoreChanged() {
  net::CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
}

void NetworkService::OnClientCertStoreChanged() {
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
}

void NetworkService::SetEncryptionKey(const std::string& encryption_key) {
  OSCrypt::SetRawEncryptionKey(encryption_key);
}

void NetworkService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::MemoryPressureListener::NotifyMemoryPressure(memory_pressure_level);
}

void NetworkService::OnPeerToPeerConnectionsCountChange(uint32_t count) {
  network_quality_estimator_manager_->GetNetworkQualityEstimator()
      ->OnPeerToPeerConnectionsCountChange(count);
}

#if BUILDFLAG(IS_ANDROID)
void NetworkService::OnApplicationStateChange(
    base::android::ApplicationState state) {
  for (NetworkContext* network_context : network_contexts_) {
    for (auto const& listener : network_context->app_status_listeners()) {
      listener->Notify(state);
    }
  }
}
#endif

void NetworkService::SetEnvironment(
    std::vector<mojom::EnvironmentVariablePtr> environment) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  for (const auto& variable : environment) {
    env->SetVar(variable->name, variable->value);
  }
}

void NetworkService::SetTrustTokenKeyCommitments(
    const std::string& raw_commitments,
    base::OnceClosure done) {
  trust_token_key_commitments_->ParseAndSet(raw_commitments);
  std::move(done).Run();
}

void NetworkService::ParseHeaders(
    const GURL& url,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    ParseHeadersCallback callback) {
  std::move(callback).Run(PopulateParsedHeaders(headers.get(), url));
}

void NetworkService::EnableDataUseUpdates(bool enable) {
  data_use_updates_enabled_ = enable;
}

void NetworkService::SetIPv6ReachabilityOverride(bool reachability_override) {
  host_resolver_manager_->SetIPv6ReachabilityOverride(reachability_override);
}

#if BUILDFLAG(IS_CT_SUPPORTED)
void NetworkService::ClearSCTAuditingCache() {
  sct_auditing_cache_->ClearCache();
}

void NetworkService::ConfigureSCTAuditing(
    mojom::SCTAuditingConfigurationPtr configuration) {
  sct_auditing_cache_->Configure(std::move(configuration));
}

void NetworkService::UpdateCtLogList(std::vector<mojom::CTLogInfoPtr> log_list,
                                     UpdateCtLogListCallback callback) {
  log_list_ = std::move(log_list);

  std::move(callback).Run();
}

void NetworkService::UpdateCtKnownPopularSCTs(
    const std::vector<std::vector<uint8_t>>& sct_hashes,
    UpdateCtLogListCallback callback) {
  sct_auditing_cache_->set_popular_scts(std::move(sct_hashes));
  std::move(callback).Run();
}

void NetworkService::SetCtEnforcementEnabled(
    bool enabled,
    SetCtEnforcementEnabledCallback callback) {
  ct_enforcement_enabled_ = enabled;
  for (NetworkContext* context : network_contexts_) {
    context->url_request_context()
        ->transport_security_state()
        ->SetCTEmergencyDisabled(!ct_enforcement_enabled_);
  }
  std::move(callback).Run();
}

#endif  // BUILDFLAG(IS_CT_SUPPORTED)

void NetworkService::UpdateKeyPinsList(mojom::PinListPtr pin_list,
                                       base::Time update_time) {
  pins_list_updated_ = true;
  pinsets_.clear();
  host_pins_.clear();
  pins_list_update_time_ = update_time;
  for (const auto& pinset : pin_list->pinsets) {
    pinsets_.emplace_back(pinset->name, pinset->static_spki_hashes,
                          pinset->bad_static_spki_hashes);
  }
  for (const auto& info : pin_list->host_pins) {
    host_pins_.emplace_back(info->hostname, info->pinset_name,
                            info->include_subdomains);
  }
  for (NetworkContext* context : network_contexts_) {
    net::TransportSecurityState* state =
        context->url_request_context()->transport_security_state();
    if (state) {
      state->UpdatePinList(pinsets_, host_pins_, pins_list_update_time_);
    }
  }
}

void NetworkService::UpdateMaskedDomainList(
    mojo_base::ProtoWrapper masked_domain_list,
    const std::vector<std::string>& exclusion_list) {
  const base::Time start_time = base::Time::Now();
  auto mdl = masked_domain_list.As<masked_domain_list::MaskedDomainList>();
  if (mdl.has_value()) {
    UMA_HISTOGRAM_MEMORY_KB("NetworkService.MaskedDomainList.SizeInKB",
                            mdl->ByteSizeLong() / 1024);

    masked_domain_list_manager_->UpdateMaskedDomainList(mdl.value(),
                                                        exclusion_list);

    base::UmaHistogramBoolean(
        "NetworkService.IpProtection.ProxyAllowList."
        "UpdateSuccess",
        true);
  } else {
    base::UmaHistogramBoolean(
        "NetworkService.IpProtection.ProxyAllowList.UpdateSuccess", false);
    LOG(ERROR) << "Unable to parse MDL in NetworkService";
  }

  base::UmaHistogramTimes(
      "NetworkService.IpProtection.ProxyAllowList.UpdateProcessTime",
      base::Time::Now() - start_time);
}

#if BUILDFLAG(IS_ANDROID)
void NetworkService::DumpWithoutCrashing(base::Time dump_request_time) {
  static base::debug::CrashKeyString* time_key =
      base::debug::AllocateCrashKeyString("time_since_dump_request_ms",
                                          base::debug::CrashKeySize::Size32);
  base::debug::ScopedCrashKeyString scoped_time(
      time_key, base::NumberToString(
                    (base::Time::Now() - dump_request_time).InMilliseconds()));
  base::debug::DumpWithoutCrashing();
}
#endif

void NetworkService::BindTestInterfaceForTesting(
    mojo::PendingReceiver<mojom::NetworkServiceTest> receiver) {
  if (registry_) {
    auto pipe = receiver.PassPipe();
    registry_->TryBindInterface(mojom::NetworkServiceTest::Name_, &pipe);
  }
}

void NetworkService::SetFirstPartySets(net::GlobalFirstPartySets sets) {
  first_party_sets_manager_->SetCompleteSets(std::move(sets));
}

void NetworkService::SetExplicitlyAllowedPorts(
    const std::vector<uint16_t>& ports) {
  net::SetExplicitlyAllowedPorts(ports);
}

#if BUILDFLAG(IS_LINUX)
void NetworkService::SetGssapiLibraryLoadObserver(
    mojo::PendingRemote<mojom::GssapiLibraryLoadObserver>
        gssapi_library_load_observer) {
  DCHECK(!gssapi_library_load_observer_.is_bound());
  gssapi_library_load_observer_.Bind(std::move(gssapi_library_load_observer));
}
#endif  // BUILDFLAG(IS_LINUX)

void NetworkService::StartNetLogBounded(base::File file,
                                        uint64_t max_total_size,
                                        net::NetLogCaptureMode capture_mode,
                                        base::Value::Dict client_constants) {
  base::Value::Dict constants = net::GetNetConstants();
  constants.Merge(std::move(client_constants));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NetLogExporter::CreateScratchDirForNetworkService,
                     base::PassKey<NetworkService>()),

      base::BindOnce(
          &NetworkService::OnStartNetLogBoundedScratchDirectoryCreated,
          weak_factory_.GetWeakPtr(), std::move(file), max_total_size,
          capture_mode, std::move(constants)));
}

void NetworkService::OnStartNetLogBoundedScratchDirectoryCreated(
    base::File file,
    uint64_t max_total_size,
    net::NetLogCaptureMode capture_mode,
    base::Value::Dict constants,
    const base::FilePath& in_progress_dir_path) {
  if (in_progress_dir_path.empty()) {
    LOG(ERROR) << "Unable to create scratch directory for net-log.";
    return;
  }

  file_net_log_observer_ = net::FileNetLogObserver::CreateBoundedPreExisting(
      in_progress_dir_path, std::move(file), max_total_size, capture_mode,
      std::make_unique<base::Value::Dict>(std::move(constants)));
  file_net_log_observer_->StartObserving(net_log_);
}

void NetworkService::StartNetLogUnbounded(base::File file,
                                          net::NetLogCaptureMode capture_mode,
                                          base::Value::Dict client_constants) {
  base::Value::Dict constants = net::GetNetConstants();
  constants.Merge(std::move(client_constants));

  file_net_log_observer_ = net::FileNetLogObserver::CreateUnboundedPreExisting(
      std::move(file), capture_mode,
      std::make_unique<base::Value::Dict>(std::move(constants)));
  file_net_log_observer_->StartObserving(net_log_);
}

std::unique_ptr<net::HttpAuthHandlerFactory>
NetworkService::CreateHttpAuthHandlerFactory(NetworkContext* network_context) {
  if (!http_auth_static_network_service_params_) {
    return net::HttpAuthHandlerFactory::CreateDefault(
        network_context->GetHttpAuthPreferences()
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_KERBEROS)
            ,
        base::BindRepeating(&CreateAuthSystem, network_context)
#endif
    );
  }

  return net::HttpAuthHandlerRegistryFactory::Create(
      network_context->GetHttpAuthPreferences()
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
          ,
      http_auth_static_network_service_params_->gssapi_library_name
#endif
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_KERBEROS)
      ,
      base::BindRepeating(&CreateAuthSystem, network_context)
#endif
  );
}

#if BUILDFLAG(IS_LINUX)
void NetworkService::OnBeforeGssapiLibraryLoad() {
  if (gssapi_library_load_observer_.is_bound()) {
    gssapi_library_load_observer_->OnBeforeGssapiLibraryLoad();
    // OnBeforeGssapiLibraryLoad() only needs to be called once.
    gssapi_library_load_observer_.reset();
  }
}
#endif  // BUILDFLAG(IS_LINUX)

void NetworkService::InitMockNetworkChangeNotifierForTesting() {
  mock_network_change_notifier_ =
      net::NetworkChangeNotifier::CreateMockIfNeeded();
}

void NetworkService::DestroyNetworkContexts() {
  owned_network_contexts_.clear();
}

void NetworkService::OnNetworkContextConnectionClosed(
    NetworkContext* network_context) {
  auto it = owned_network_contexts_.find(network_context);
  CHECK(it != owned_network_contexts_.end(), base::NotFatalUntil::M130);
  if (file_net_log_observer_) {
    net_log_polled_data_list_.Append(
        net::GetNetInfo(network_context->url_request_context()));
  }
  owned_network_contexts_.erase(it);
}

void NetworkService::Bind(
    mojo::PendingReceiver<mojom::NetworkService> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

mojom::URLLoaderNetworkServiceObserver*
NetworkService::GetDefaultURLLoaderNetworkServiceObserver() {
  if (default_url_loader_network_service_observer_) {
    return default_url_loader_network_service_observer_.get();
  }
  return nullptr;
}

// static
NetworkService* NetworkService::GetNetworkServiceForTesting() {
  return g_network_service;
}

void NetworkService::SetTpcdMetadataGrants(
    const std::vector<ContentSettingPatternSource>& settings) {
  tpcd_metadata_manager_->SetGrants(settings);
}
}  // namespace network
