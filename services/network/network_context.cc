// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/domain_reliability/features.h"
#include "components/domain_reliability/monitor.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/dns/host_cache.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/cookie_manager.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/data_remover_util.h"
#include "services/network/disk_cache/mojo_backend_file_operations_factory.h"
#include "services/network/host_resolver.h"
#include "services/network/http_auth_cache_copier.h"
#include "services/network/http_server_properties_pref_delegate.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/ip_protection_config_cache_impl.h"
#include "services/network/is_browser_initiated.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_service.h"
#include "services/network/network_service_memory_cache.h"
#include "services/network/network_service_network_delegate.h"
#include "services/network/network_service_proxy_delegate.h"
#include "services/network/oblivious_http_request_handler.h"
#include "services/network/proxy_config_service_mojo.h"
#include "services/network/proxy_lookup_request.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/public/cpp/cert_verifier/mojo_cert_verifier.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/cpp/thread_delegate.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/reporting_service.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resolve_host_request.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_network_transaction_factory.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/ssl_config_service_mojo.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_transaction_factory.h"
#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/sqlite_trust_token_persister.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_query_answerer.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "services/network/web_transport.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "components/certificate_transparency/chrome_require_ct_delegate.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "net/cert/cert_and_ct_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "services/network/ct_log_list_distributor.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_handler.h"
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if BUILDFLAG(IS_CHROMEOS)
#include "services/network/cert_verifier_with_trust_anchors.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_WEBSOCKETS)
#include "services/network/websocket_factory.h"
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/base/http_user_agent_settings.h"
#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(ENABLE_MDNS)
#include "services/network/mdns_responder.h"
#endif  // BUILDFLAG(ENABLE_MDNS)

#if BUILDFLAG(IS_P2P_ENABLED)
#include "services/network/p2p/socket_manager.h"
#endif  // BUILDFLAG(IS_P2P_ENABLED)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace network {

namespace {

net::CertVerifier* g_cert_verifier_for_testing = nullptr;

// A CertVerifier that forwards all requests to |g_cert_verifier_for_testing|.
// This is used to allow NetworkContexts to have their own
// std::unique_ptr<net::CertVerifier> while forwarding calls to the shared
// verifier.
class WrappedTestingCertVerifier : public net::CertVerifier {
 public:
  ~WrappedTestingCertVerifier() override = default;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (!g_cert_verifier_for_testing)
      return net::ERR_FAILED;
    return g_cert_verifier_for_testing->Verify(
        params, verify_result, std::move(callback), out_req, net_log);
  }
  void SetConfig(const Config& config) override {
    if (!g_cert_verifier_for_testing)
      return;
    g_cert_verifier_for_testing->SetConfig(config);
  }
  void AddObserver(Observer* observer) override {
    if (!g_cert_verifier_for_testing) {
      return;
    }
    g_cert_verifier_for_testing->AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    if (!g_cert_verifier_for_testing) {
      return;
    }
    g_cert_verifier_for_testing->RemoveObserver(observer);
  }
};

// Predicate function to determine if the given |domain| matches the
// |filter_type| and |filter_domains| from a |mojom::ClearDataFilter|.
bool MatchesDomainFilter(mojom::ClearDataFilter_Type filter_type,
                         std::set<std::string> filter_domains,
                         const std::string& domain) {
  bool found_domain = filter_domains.find(domain) != filter_domains.end();
  return (filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES) ==
         found_domain;
}

// Returns a callback that checks if a domain matches the |filter|. |filter|
// must contain no origins. A null filter matches everything.
base::RepeatingCallback<bool(const std::string& host_name)> MakeDomainFilter(
    mojom::ClearDataFilter* filter) {
  if (!filter)
    return base::BindRepeating([](const std::string&) { return true; });

  DCHECK(filter->origins.empty())
      << "Origin filtering not allowed in a domain-only filter";

  std::set<std::string> filter_domains;
  filter_domains.insert(filter->domains.begin(), filter->domains.end());
  return base::BindRepeating(&MatchesDomainFilter, filter->type,
                             std::move(filter_domains));
}

// Predicate function to determine if the given |origin| matches the
// |filter_type|, |filter_domains| and |filter_origins| from a
// |mojom::ClearDataFilter|.
bool MatchesOriginFilter(mojom::ClearDataFilter_Type filter_type,
                         std::set<std::string> filter_domains,
                         std::set<url::Origin> filter_origins,
                         const url::Origin& origin) {
  std::string url_registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain =
      (filter_domains.find(url_registrable_domain != ""
                               ? url_registrable_domain
                               : origin.host()) != filter_domains.end());

  bool found_origin = (filter_origins.find(origin) != filter_origins.end());

  return (filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES) ==
         (found_domain || found_origin);
}

// Builds a generic Origin-matching predicate function based on |filter|. If
// |filter| is null, creates an always-true predicate.
base::RepeatingCallback<bool(const url::Origin&)> BuildOriginFilter(
    mojom::ClearDataFilterPtr filter) {
  if (!filter) {
    return base::BindRepeating([](const url::Origin&) { return true; });
  }

  std::set<std::string> filter_domains;
  filter_domains.insert(filter->domains.begin(), filter->domains.end());

  std::set<url::Origin> filter_origins;
  filter_origins.insert(filter->origins.begin(), filter->origins.end());

  return base::BindRepeating(&MatchesOriginFilter, filter->type,
                             std::move(filter_domains),
                             std::move(filter_origins));
}

#if BUILDFLAG(IS_ANDROID)
class NetworkContextApplicationStatusListener
    : public base::android::ApplicationStatusListener {
 public:
  // base::android::ApplicationStatusListener implementation:
  void SetCallback(const ApplicationStateChangeCallback& callback) override {
    DCHECK(!callback_);
    DCHECK(callback);
    callback_ = callback;
  }

  void Notify(base::android::ApplicationState state) override {
    if (callback_)
      callback_.Run(state);
  }

 private:
  ApplicationStateChangeCallback callback_;
};
#endif  // BUILDFLAG(IS_ANDROID)

struct TestVerifyCertState {
  net::CertVerifyResult result;
  std::unique_ptr<net::CertVerifier::Request> request;
};

void TestVerifyCertCallback(
    std::unique_ptr<TestVerifyCertState> request,
    NetworkContext::VerifyCertificateForTestingCallback callback,
    int result) {
  std::move(callback).Run(result);
}

std::string HashesToBase64String(const net::HashValueVector& hashes) {
  std::string str;
  for (size_t i = 0; i != hashes.size(); ++i) {
    if (i != 0)
      str += ",";
    str += hashes[i].ToString();
  }
  return str;
}

#if BUILDFLAG(IS_CT_SUPPORTED)
// SCTAuditingDelegate is an implementation of the delegate interface that is
// aware of per-NetworkContext details (to allow the cache to notify the
// associated NetworkContextClient of new reports, and to apply
// per-NetworkContext enabled/disabled status for the auditing feature).
class SCTAuditingDelegate : public net::SCTAuditingDelegate {
 public:
  explicit SCTAuditingDelegate(const base::WeakPtr<NetworkContext>& context);
  ~SCTAuditingDelegate() override;

  // net::SCTAuditingDelegate:
  void MaybeEnqueueReport(
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps) override;

 private:
  base::WeakPtr<NetworkContext> context_;
};

SCTAuditingDelegate::SCTAuditingDelegate(
    const base::WeakPtr<NetworkContext>& context)
    : context_(context) {}

SCTAuditingDelegate::~SCTAuditingDelegate() = default;

void SCTAuditingDelegate::MaybeEnqueueReport(
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (!context_)
    return;
  context_->MaybeEnqueueSCTReport(host_port_pair, validated_certificate_chain,
                                  signed_certificate_timestamps);
}

// Filters `log_list` for disqualified or Google-operated logs,
// returning them as sorted vectors in `disqualified_logs` and
// `operated_by_google_logs` suitable for use with a `CTPolicyEnforcer`.
void GetCTPolicyConfigForCTLogInfo(
    const std::vector<mojom::CTLogInfoPtr>& log_list,
    std::vector<std::pair<std::string, base::Time>>* disqualified_logs,
    std::vector<std::string>* operated_by_google_logs,
    std::map<std::string, certificate_transparency::OperatorHistoryEntry>*
        operator_history) {
  for (const auto& log : log_list) {
    std::string log_id = crypto::SHA256HashString(log->public_key);
    if (log->operated_by_google || log->disqualified_at) {
      if (log->operated_by_google)
        operated_by_google_logs->push_back(log_id);
      if (log->disqualified_at) {
        disqualified_logs->emplace_back(log_id, log->disqualified_at.value());
      }
    }
    certificate_transparency::OperatorHistoryEntry entry;
    entry.current_operator_ = log->current_operator;
    for (const auto& previous_operator : log->previous_operators) {
      entry.previous_operators_.emplace_back(previous_operator->name,
                                             previous_operator->end_time);
    }
    (*operator_history)[log_id] = entry;
  }

  std::sort(std::begin(*operated_by_google_logs),
            std::end(*operated_by_google_logs));
  std::sort(std::begin(*disqualified_logs), std::end(*disqualified_logs));
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

// Obtains a full data file path from a NetworkContextFilePaths, a class member
// pointer to the data file. If valid, then returns true and places the full
// path into `full_path` otherwise returns false.
bool GetFullDataFilePath(
    const mojom::NetworkContextFilePathsPtr& file_paths,
    absl::optional<base::FilePath> network::mojom::NetworkContextFilePaths::*
        field_name,
    base::FilePath& full_path) {
  if (!file_paths)
    return false;

  absl::optional<base::FilePath> relative_file_path =
      file_paths.get()->*field_name;
  if (!relative_file_path.has_value())
    return false;

  // Path to a data file should always be a plain filename.
  DCHECK_EQ(relative_file_path->BaseName(), *relative_file_path);

  full_path =
      file_paths->data_directory.path().Append(relative_file_path->value());
  return true;
}

}  // namespace

constexpr uint32_t NetworkContext::kMaxOutstandingRequestsPerProcess;

NetworkContext::NetworkContextHttpAuthPreferences::
    NetworkContextHttpAuthPreferences(NetworkService* network_service)
    : network_service_(network_service) {}

NetworkContext::NetworkContextHttpAuthPreferences::
    ~NetworkContextHttpAuthPreferences() = default;

#if BUILDFLAG(IS_LINUX)
bool NetworkContext::NetworkContextHttpAuthPreferences::AllowGssapiLibraryLoad()
    const {
  if (network_service_) {
    network_service_->OnBeforeGssapiLibraryLoad();
  }
  return net::HttpAuthPreferences::AllowGssapiLibraryLoad();
}
#endif  // BUILDFLAG(IS_LINUX)

NetworkContext::PendingCertVerify::PendingCertVerify() = default;
NetworkContext::PendingCertVerify::~PendingCertVerify() = default;

NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    mojom::NetworkContextParamsPtr params,
    OnConnectionCloseCallback on_connection_close_callback)
    : NetworkContext(base::PassKey<NetworkContext>(),
                     network_service,
                     std::move(receiver),
                     std::move(params),
                     std::move(on_connection_close_callback),
                     OnURLRequestContextBuilderConfiguredCallback()) {}

// net::NetworkDelegate that wraps
NetworkContext::NetworkContext(
    base::PassKey<NetworkContext> pass_key,
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    mojom::NetworkContextParamsPtr params,
    OnConnectionCloseCallback on_connection_close_callback,
    OnURLRequestContextBuilderConfiguredCallback
        on_url_request_context_builder_configured)
    : network_service_(network_service),
      url_request_context_(nullptr),
#if BUILDFLAG(ENABLE_REPORTING)
      is_observing_reporting_service_(false),
#endif  // BUILDFLAG(ENABLE_REPORTING)
      params_(std::move(params)),
      on_connection_close_callback_(std::move(on_connection_close_callback)),
      receiver_(this, std::move(receiver)),
      first_party_sets_access_delegate_(
          std::move(params_->first_party_sets_access_delegate_receiver),
          std::move(params_->first_party_sets_access_delegate_params),
          network_service_->first_party_sets_manager()),
      cors_preflight_controller_(network_service),
      http_auth_merged_preferences_(network_service),
      ohttp_handler_(this),
      cors_non_wildcard_request_headers_support_(base::FeatureList::IsEnabled(
          features::kCorsNonWildcardRequestHeadersSupport)) {
#if BUILDFLAG(IS_WIN) && DCHECK_IS_ON()
  if (params_->file_paths) {
    DCHECK(params_->win_permissions_set)
        << "Permissions not set on files. Network context should be created "
           "using CreateNetworkContextInNetworkService rather than directly on "
           "the network service.";
  }
#endif  // BUILDFLAG(IS_WIN) && DCHECK_IS_ON()

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
  if (params_->file_paths) {
    if (params_->file_paths->http_cache_directory) {
      EnsureMounted(&*params_->file_paths->http_cache_directory);
    }
    if (params_->file_paths->shared_dictionary_directory) {
      EnsureMounted(&*params_->file_paths->shared_dictionary_directory);
    }
    EnsureMounted(&params_->file_paths->data_directory);
  }
#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

  if (params_->shared_dictionary_enabled) {
    if (params_->file_paths &&
        params_->file_paths->shared_dictionary_directory &&
        !params_->file_paths->shared_dictionary_directory->path().empty()) {
#if BUILDFLAG(IS_ANDROID)
      app_status_listeners_.push_back(
          std::make_unique<NetworkContextApplicationStatusListener>());
#endif  // BUILDFLAG(IS_ANDROID)
      // TODO(crbug.com/1413922): Set `file_operations_factory` to support
      // sandboxed network service on Android.
      shared_dictionary_manager_ = SharedDictionaryManager::CreateOnDisk(
          params_->file_paths->shared_dictionary_directory->path().Append(
              FILE_PATH_LITERAL("db")),
          params_->file_paths->shared_dictionary_directory->path().Append(
              FILE_PATH_LITERAL("cache")),
          params_->shared_dictionary_cache_max_size,
          shared_dictionary::kDictionaryMaxCountPerNetworkContext,
#if BUILDFLAG(IS_ANDROID)
          app_status_listeners_.rbegin()->get(),
#endif  // BUILDFLAG(IS_ANDROID)
          /*file_operations_factory=*/nullptr);
    } else {
      shared_dictionary_manager_ = SharedDictionaryManager::CreateInMemory(
          params_->shared_dictionary_cache_max_size,
          shared_dictionary::kDictionaryMaxCountPerNetworkContext);
    }
  }

  mojo::PendingRemote<mojom::URLLoaderFactory>
      url_loader_factory_for_cert_net_fetcher;
  mojo::PendingReceiver<mojom::URLLoaderFactory>
      url_loader_factory_for_cert_net_fetcher_receiver =
          url_loader_factory_for_cert_net_fetcher
              .InitWithNewPipeAndPassReceiver();

  scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store =
      MakeSessionCleanupCookieStore();

  url_request_context_owner_ = MakeURLRequestContext(
      std::move(url_loader_factory_for_cert_net_fetcher),
      session_cleanup_cookie_store,
      std::move(on_url_request_context_builder_configured));
  url_request_context_ = url_request_context_owner_.url_request_context.get();

  cookie_manager_ = std::make_unique<CookieManager>(
      url_request_context_, &first_party_sets_access_delegate_,
      std::move(session_cleanup_cookie_store),
      std::move(params_->cookie_manager_params));

  cookie_manager_->AddSettingsWillChangeCallback(
      base::BindRepeating(&NetworkContext::OnCookieManagerSettingsChanged,
                          weak_factory_.GetWeakPtr()));

  network_service_->RegisterNetworkContext(this);

  // Only register for destruction if |this| will be wholly lifetime-managed
  // by the NetworkService. In the other constructors, lifetime is shared with
  // other consumers, and thus self-deletion is not safe and can result in
  // double-frees.
  receiver_.set_disconnect_handler(base::BindOnce(
      &NetworkContext::OnConnectionError, base::Unretained(this)));

  socket_factory_ = std::make_unique<SocketFactory>(
      url_request_context_->net_log(), url_request_context_);
  resource_scheduler_ = std::make_unique<ResourceScheduler>();

  if (base::FeatureList::IsEnabled(features::kNetworkServiceMemoryCache))
    memory_cache_ = std::make_unique<NetworkServiceMemoryCache>(this);

  if (params_->http_auth_static_network_context_params) {
    http_auth_merged_preferences_.SetAllowDefaultCredentials(
        params_->http_auth_static_network_context_params
            ->allow_default_credentials);
  }

  InitializeCorsParams();

  SetSplitAuthCacheByNetworkAnonymizationKey(
      params_->split_auth_cache_by_network_anonymization_key);

#if BUILDFLAG(IS_CT_SUPPORTED)
  if (params_->ct_policy)
    SetCTPolicy(std::move(params_->ct_policy));

  base::FilePath sct_auditing_path;
  GetFullDataFilePath(params_->file_paths,
                      &network::mojom::NetworkContextFilePaths::
                          sct_auditing_pending_reports_file_name,
                      sct_auditing_path);
  sct_auditing_handler_ =
      std::make_unique<SCTAuditingHandler>(this, sct_auditing_path);
  sct_auditing_handler()->SetMode(params_->sct_auditing_mode);
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if BUILDFLAG(IS_ANDROID)
  if (params_->cookie_manager)
    GetCookieManager(std::move(params_->cookie_manager));
#endif  // BUILDFLAG(IS_ANDROID)

  CreateURLLoaderFactoryForCertNetFetcher(
      std::move(url_loader_factory_for_cert_net_fetcher_receiver));

  SetBlockTrustTokens(params_->block_trust_tokens);

  if (params_ && params_->http_cache_file_operations_factory) {
    http_cache_file_operations_factory_ =
        base::MakeRefCounted<MojoBackendFileOperationsFactory>(
            std::move(params_->http_cache_file_operations_factory));
  }
}

NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    net::URLRequestContext* url_request_context,
    const std::vector<std::string>& cors_exempt_header_list)
    : network_service_(network_service),
      url_request_context_(url_request_context),
#if BUILDFLAG(ENABLE_REPORTING)
      is_observing_reporting_service_(false),
#endif  // BUILDFLAG(ENABLE_REPORTING)
      receiver_(this, std::move(receiver)),
      first_party_sets_access_delegate_(
          /*receiver=*/mojo::NullReceiver(),
          /*params=*/nullptr,
          /*manager=*/nullptr),
      cookie_manager_(std::make_unique<CookieManager>(
          url_request_context,
          nullptr,
          /*first_party_sets_access_delegate=*/nullptr,
          nullptr)),
      socket_factory_(
          std::make_unique<SocketFactory>(url_request_context_->net_log(),
                                          url_request_context)),
      cors_preflight_controller_(network_service),
      http_auth_merged_preferences_(network_service),
      ohttp_handler_(this) {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->RegisterNetworkContext(this);
  resource_scheduler_ = std::make_unique<ResourceScheduler>();

  for (const auto& key : cors_exempt_header_list)
    cors_exempt_header_list_.insert(key);

  acam_preflight_spec_conformant_ = base::FeatureList::IsEnabled(
      network::features::
          kAccessControlAllowMethodsInCORSPreflightSpecConformant);
}

NetworkContext::~NetworkContext() {
  is_destructing_ = true;

  // May be nullptr in tests.
  if (network_service_) {
#if BUILDFLAG(IS_ANDROID)
    if (params_ && params_->file_paths) {
      base::FilePath path_to_invalidate;
      if (GetFullDataFilePath(params_->file_paths,
                              &network::mojom::NetworkContextFilePaths::
                                  trust_token_database_name,
                              path_to_invalidate)) {
        network_service_->InvalidateNetworkContextPath(path_to_invalidate);
      }
      if (GetFullDataFilePath(params_->file_paths,
                              &network::mojom::NetworkContextFilePaths::
                                  reporting_and_nel_store_database_name,
                              path_to_invalidate)) {
        network_service_->InvalidateNetworkContextPath(path_to_invalidate);
      }
      if (GetFullDataFilePath(
              params_->file_paths,
              &network::mojom::NetworkContextFilePaths::cookie_database_name,
              path_to_invalidate)) {
        network_service_->InvalidateNetworkContextPath(path_to_invalidate);
      }
    }

#endif
    network_service_->DeregisterNetworkContext(this);
  }
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->Shutdown();
  // Because of the order of declaration in the class,
  // domain_reliability_monitor_ will be destroyed before
  // |url_loader_factories_| which could own URLLoader's whose destructor call
  // back into this class and might use domain_reliability_monitor_. So we reset
  // |domain_reliability_monitor_| here explicitly, instead of changing the
  // order, because any work calling into |domain_reliability_monitor_| at
  // shutdown would be unnecessary as the reports would be thrown out.
  domain_reliability_monitor_.reset();

  if (url_request_context_ &&
      url_request_context_->transport_security_state()) {
    if (certificate_report_sender_) {
      // Destroy |certificate_report_sender_| before |url_request_context_|,
      // since the former has a reference to the latter.
      url_request_context_->transport_security_state()->SetReportSender(
          nullptr);
      certificate_report_sender_.reset();
    }

#if BUILDFLAG(IS_CT_SUPPORTED)
    if (require_ct_delegate_) {
      url_request_context_->transport_security_state()->SetRequireCTDelegate(
          nullptr);
    }
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (is_observing_reporting_service_) {
    DCHECK(url_request_context());
    // May be nullptr in tests.
    if (url_request_context()->reporting_service()) {
      url_request_context()->reporting_service()->RemoveReportingCacheObserver(
          this);
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
  if (!dismount_closures_.empty()) {
    // Dismount all mounted directories after a generous delay, so that
    // pending asynchronous IO tasks have a chance to complete before the
    // directory is unmounted.
    constexpr base::TimeDelta kDismountDelay = base::Minutes(5);

    for (auto& dismount_closure : dismount_closures_) {
      std::ignore = base::ThreadPool::PostDelayedTask(
          FROM_HERE, std::move(dismount_closure), kDismountDelay);
    }
  }
#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

  // Clear `url_loader_factories_` before deleting the contents, as it can
  // result in re-entrant calls to DestroyURLLoaderFactory().
  std::set<std::unique_ptr<cors::CorsURLLoaderFactory>,
           base::UniquePtrComparator>
      url_loader_factories = std::move(url_loader_factories_);
}

void NetworkContext::OnCookieManagerSettingsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const std::unique_ptr<network::RestrictedCookieManager>& rcm :
       restricted_cookie_managers_) {
    rcm->OnCookieSettingsChanged();
  }
}

// static
std::unique_ptr<NetworkContext> NetworkContext::CreateForTesting(
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    mojom::NetworkContextParamsPtr params,
    OnURLRequestContextBuilderConfiguredCallback
        on_url_request_context_builder_configured) {
  return std::make_unique<NetworkContext>(
      base::PassKey<NetworkContext>(), network_service, std::move(receiver),
      std::move(params), OnConnectionCloseCallback(),
      std::move(on_url_request_context_builder_configured));
}

// static
void NetworkContext::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

void NetworkContext::CreateURLLoaderFactory(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client) {
  url_loader_factories_.emplace(std::make_unique<cors::CorsURLLoaderFactory>(
      this, std::move(params), std::move(resource_scheduler_client),
      std::move(receiver), &cors_origin_access_list_,
      network_service_ ? network_service_->network_service_resource_block_list()
                       : nullptr));
}

void NetworkContext::CreateURLLoaderFactoryForCertNetFetcher(
    mojo::PendingReceiver<mojom::URLLoaderFactory> factory_receiver) {
  // TODO(crbug.com/1087790): investigate changing these params.
  auto url_loader_factory_params = mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->is_trusted = true;
  url_loader_factory_params->process_id = mojom::kBrowserProcessId;
  url_loader_factory_params->automatically_assign_isolation_info = true;
  url_loader_factory_params->is_corb_enabled = false;
  CreateURLLoaderFactory(std::move(factory_receiver),
                         std::move(url_loader_factory_params));
}

void NetworkContext::ActivateDohProbes() {
  DCHECK(url_request_context_->host_resolver());

  doh_probes_request_.reset();
  doh_probes_request_ =
      url_request_context_->host_resolver()->CreateDohProbeRequest();
  doh_probes_request_->Start();
}

void NetworkContext::SetClient(
    mojo::PendingRemote<mojom::NetworkContextClient> client) {
  client_.reset();
  client_.Bind(std::move(client));
}

void NetworkContext::CreateURLLoaderFactory(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
    mojom::URLLoaderFactoryParamsPtr params) {
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client =
      base::MakeRefCounted<ResourceSchedulerClient>(
          ResourceScheduler::ClientId::Create(params->top_frame_id),
          IsBrowserInitiated(params->process_id == mojom::kBrowserProcessId),
          resource_scheduler_.get(),
          url_request_context_->network_quality_estimator());
  CreateURLLoaderFactory(std::move(receiver), std::move(params),
                         std::move(resource_scheduler_client));
}

void NetworkContext::ResetURLLoaderFactories() {
  // Move all factories to a temporary vector so ClearBindings() does not
  // invalidate the iterator if the factory gets deleted.
  std::vector<cors::CorsURLLoaderFactory*> factories;
  factories.reserve(url_loader_factories_.size());
  for (const auto& factory : url_loader_factories_)
    factories.push_back(factory.get());
  for (auto* factory : factories)
    factory->ClearBindings();
}

void NetworkContext::GetViaObliviousHttp(
    mojom::ObliviousHttpRequestPtr request,
    mojo::PendingRemote<mojom::ObliviousHttpClient> client) {
  ohttp_handler_.StartRequest(std::move(request), std::move(client));
}

void NetworkContext::GetCookieManager(
    mojo::PendingReceiver<mojom::CookieManager> receiver) {
  cookie_manager_->AddReceiver(std::move(receiver));
}

void NetworkContext::GetRestrictedCookieManager(
    mojo::PendingReceiver<mojom::RestrictedCookieManager> receiver,
    mojom::RestrictedCookieManagerRole role,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer) {
  RestrictedCookieManager::ComputeFirstPartySetMetadata(
      origin, url_request_context_->cookie_store(), isolation_info,
      base::BindOnce(&NetworkContext::OnComputedFirstPartySetMetadata,
                     weak_factory_.GetWeakPtr(), std::move(receiver), role,
                     origin, isolation_info, cookie_setting_overrides,
                     std::move(cookie_observer)));
}

void NetworkContext::OnRCMDisconnect(
    const network::RestrictedCookieManager* rcm) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = restricted_cookie_managers_.find(rcm);
  DCHECK(it != restricted_cookie_managers_.end());
  restricted_cookie_managers_.erase(it);
}

void NetworkContext::OnComputedFirstPartySetMetadata(
    mojo::PendingReceiver<mojom::RestrictedCookieManager> receiver,
    mojom::RestrictedCookieManagerRole role,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
    net::FirstPartySetMetadata first_party_set_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RestrictedCookieManager> ptr =
      std::make_unique<RestrictedCookieManager>(
          role, url_request_context_->cookie_store(),
          cookie_manager_->cookie_settings(), origin, isolation_info,
          cookie_setting_overrides, std::move(cookie_observer),
          std::move(first_party_set_metadata),
          network_service_->metrics_updater());

  auto callback = base::BindOnce(&NetworkContext::OnRCMDisconnect,
                                 base::Unretained(this), ptr.get());
  ptr->InstallReceiver(std::move(receiver),
                       ThreadDelegate::GetHighPriorityTaskRunner(),
                       std::move(callback));
  restricted_cookie_managers_.insert(std::move(ptr));
}

void NetworkContext::GetTrustTokenQueryAnswerer(
    mojo::PendingReceiver<mojom::TrustTokenQueryAnswerer> receiver,
    const url::Origin& top_frame_origin) {
  // Only called when Trust Tokens is enabled, i.e. trust_token_store_ is
  // non-null.
  DCHECK(trust_token_store_);
  DCHECK(network_service_);

  absl::optional<SuitableTrustTokenOrigin> suitable_top_frame_origin =
      SuitableTrustTokenOrigin::Create(top_frame_origin);

  const SynchronousTrustTokenKeyCommitmentGetter* const key_commitment_getter =
      network_service_->trust_token_key_commitments();

  // It's safe to dereference |suitable_top_frame_origin| here as, during the
  // process of vending the TrustTokenQueryAnswerer, the browser ensures that
  // the requesting context's top frame origin is suitable for Trust Tokens.
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      std::move(*suitable_top_frame_origin), trust_token_store_.get(),
      key_commitment_getter);

  trust_token_query_answerers_.Add(std::move(answerer), std::move(receiver));
}

void NetworkContext::GetStoredTrustTokenCounts(
    GetStoredTrustTokenCountsCallback callback) {
  if (trust_token_store_) {
    auto get_trust_token_counts_from_store =
        [](NetworkContext::GetStoredTrustTokenCountsCallback callback,
           TrustTokenStore* trust_token_store) {
          std::vector<mojom::StoredTrustTokensForIssuerPtr> result;
          for (auto& issuer_count_pair :
               trust_token_store->GetStoredTrustTokenCounts()) {
            result.push_back(mojom::StoredTrustTokensForIssuer::New(
                std::move(issuer_count_pair.first), issuer_count_pair.second));
          }
          std::move(callback).Run(std::move(result));
        };
    trust_token_store_->ExecuteOrEnqueue(
        base::BindOnce(get_trust_token_counts_from_store, std::move(callback)));
  } else {
    // The Trust Tokens feature is disabled, return immediately with an empty
    // vector.
    std::move(callback).Run({});
  }
}

void NetworkContext::DeleteStoredTrustTokens(
    const url::Origin& issuer,
    DeleteStoredTrustTokensCallback callback) {
  if (!trust_token_store_) {
    std::move(callback).Run(
        mojom::DeleteStoredTrustTokensStatus::kFailureFeatureDisabled);
    return;
  }

  absl::optional<SuitableTrustTokenOrigin> suitable_issuer_origin =
      SuitableTrustTokenOrigin::Create(issuer);
  if (!suitable_issuer_origin) {
    std::move(callback).Run(
        mojom::DeleteStoredTrustTokensStatus::kFailureInvalidOrigin);
    return;
  }

  trust_token_store_->ExecuteOrEnqueue(base::BindOnce(
      [](SuitableTrustTokenOrigin issuer,
         DeleteStoredTrustTokensCallback callback, TrustTokenStore* store) {
        const bool did_delete_tokens = store->DeleteStoredTrustTokens(issuer);
        const auto status =
            did_delete_tokens
                ? mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted
                : mojom::DeleteStoredTrustTokensStatus::kSuccessNoTokensDeleted;
        std::move(callback).Run(status);
      },
      std::move(*suitable_issuer_origin), std::move(callback)));
}

void NetworkContext::SetBlockTrustTokens(bool block) {
  block_trust_tokens_ = block;
}

void NetworkContext::OnProxyLookupComplete(
    ProxyLookupRequest* proxy_lookup_request) {
  auto it = proxy_lookup_requests_.find(proxy_lookup_request);
  DCHECK(it != proxy_lookup_requests_.end());
  proxy_lookup_requests_.erase(it);
}

void NetworkContext::DisableQuic() {
  url_request_context_->http_transaction_factory()->GetSession()->DisableQuic();
}

void NetworkContext::DestroyURLLoaderFactory(
    cors::CorsURLLoaderFactory* url_loader_factory) {
  if (is_destructing_) {
    return;
  }
  auto it = url_loader_factories_.find(url_loader_factory);
  DCHECK(it != url_loader_factories_.end());
  url_loader_factories_.erase(it);
}

void NetworkContext::Remove(WebTransport* transport) {
  auto it = web_transports_.find(transport);
  if (it != web_transports_.end()) {
    web_transports_.erase(it);
  }
}

void NetworkContext::LoaderCreated(uint32_t process_id) {
  loader_count_per_process_[process_id] += 1;
}

void NetworkContext::LoaderDestroyed(uint32_t process_id) {
  auto it = loader_count_per_process_.find(process_id);
  DCHECK(it != loader_count_per_process_.end());
  it->second -= 1;
  if (it->second == 0)
    loader_count_per_process_.erase(it);
}

bool NetworkContext::CanCreateLoader(uint32_t process_id) {
  auto it = loader_count_per_process_.find(process_id);
  uint32_t count = (it == loader_count_per_process_.end() ? 0 : it->second);
  return count < max_loaders_per_process_;
}

size_t NetworkContext::GetNumOutstandingResolveHostRequestsForTesting() const {
  size_t sum = 0;
  if (internal_host_resolver_)
    sum += internal_host_resolver_->GetNumOutstandingRequestsForTesting();
  for (const auto& host_resolver : host_resolvers_)
    sum += host_resolver.first->GetNumOutstandingRequestsForTesting();
  return sum;
}

bool NetworkContext::SkipReportingPermissionCheck() const {
#if BUILDFLAG(ENABLE_REPORTING)
  return params_ && params_->skip_reporting_send_permission_check;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void NetworkContext::ClearTrustTokenData(mojom::ClearDataFilterPtr filter,
                                         base::OnceClosure done) {
  if (!trust_token_store_) {
    std::move(done).Run();
    return;
  }
  trust_token_store_->ExecuteOrEnqueue(base::BindOnce(
      [](mojom::ClearDataFilterPtr filter, base::OnceClosure done,
         TrustTokenStore* store) {
        std::ignore = store->ClearDataForFilter(std::move(filter));
        std::move(done).Run();
      },
      std::move(filter), std::move(done)));
}

void NetworkContext::ClearTrustTokenSessionOnlyData(
    ClearTrustTokenSessionOnlyDataCallback callback) {
  // Only called when Private State Tokens is enabled, i.e.,
  // `trust_token_store_` is non-null.
  DCHECK(trust_token_store_);
  DCHECK(cookie_manager_);

  DeleteCookiePredicate cookie_predicate =
      cookie_manager_->cookie_settings().CreateDeleteCookieOnExitPredicate();

  auto store_predicate = base::BindRepeating(
      [](DeleteCookiePredicate predicate, const std::string& origin) {
        return predicate.Run(origin, true);
      },
      std::move(cookie_predicate));
  trust_token_store_->ExecuteOrEnqueue(base::BindOnce(
      [](base::RepeatingCallback<bool(const std::string&)> pred,
         ClearTrustTokenSessionOnlyDataCallback cb, TrustTokenStore* store) {
        bool any_data_deleted = store->ClearDataForPredicate(std::move(pred));
        std::move(cb).Run(any_data_deleted);
      },
      std::move(store_predicate), std::move(callback)));
}

void NetworkContext::ClearNetworkingHistoryBetween(
    base::Time start_time,
    base::Time end_time,
    base::OnceClosure completion_callback) {
#if BUILDFLAG(IS_CT_SUPPORTED)
  auto barrier = base::BarrierClosure(3, std::move(completion_callback));
  sct_auditing_handler()->ClearPendingReports(barrier);
#else
  auto barrier = base::BarrierClosure(2, std::move(completion_callback));
#endif  // BUIDLFLAG(IS_CT_SUPPORTED)

  url_request_context_->transport_security_state()->DeleteAllDynamicDataBetween(
      start_time, end_time, barrier);

  // TODO(mmenke): Neither of these methods waits until the changes have been
  // commited to disk. They probably should, as most similar methods net/
  // exposes do.
  // May not be set in all tests.
  if (network_qualities_pref_delegate_)
    network_qualities_pref_delegate_->ClearPrefs();

  url_request_context_->http_server_properties()->Clear(barrier);
}

void NetworkContext::ClearHttpCache(base::Time start_time,
                                    base::Time end_time,
                                    mojom::ClearDataFilterPtr filter,
                                    ClearHttpCacheCallback callback) {
  // It's safe to use Unretained below as the HttpCacheDataRemover is owned by
  // |this| and guarantees it won't call its callback if deleted.
  http_cache_data_removers_.push_back(HttpCacheDataRemover::CreateAndStart(
      url_request_context_, std::move(filter), start_time, end_time,
      base::BindOnce(&NetworkContext::OnHttpCacheCleared,
                     base::Unretained(this), std::move(callback))));

  NetworkServiceMemoryCache* memory_cache = GetMemoryCache();
  if (memory_cache)
    memory_cache->Clear();
}

void NetworkContext::ComputeHttpCacheSize(
    base::Time start_time,
    base::Time end_time,
    ComputeHttpCacheSizeCallback callback) {
  // It's safe to use Unretained below as the HttpCacheDataCounter is owned by
  // |this| and guarantees it won't call its callback if deleted.
  http_cache_data_counters_.push_back(HttpCacheDataCounter::CreateAndStart(
      url_request_context_, start_time, end_time,
      base::BindOnce(&NetworkContext::OnHttpCacheSizeComputed,
                     base::Unretained(this), std::move(callback))));
}

void NetworkContext::ClearCorsPreflightCache(
    mojom::ClearDataFilterPtr filter,
    ClearCorsPreflightCacheCallback callback) {
  cors_preflight_controller_.ClearCorsPreflightCache(std::move(filter));
  std::move(callback).Run();
}

void NetworkContext::ClearHostCache(mojom::ClearDataFilterPtr filter,
                                    ClearHostCacheCallback callback) {
  net::HostCache* host_cache =
      url_request_context_->host_resolver()->GetHostCache();
  DCHECK(host_cache);
  host_cache->ClearForHosts(MakeDomainFilter(filter.get()));
  std::move(callback).Run();
}

void NetworkContext::ClearHttpAuthCache(base::Time start_time,
                                        base::Time end_time,
                                        ClearHttpAuthCacheCallback callback) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);

  http_session->http_auth_cache()->ClearEntriesAddedBetween(start_time,
                                                            end_time);
  // TODO(mmenke): Use another error code for this, as ERR_ABORTED has somewhat
  // magical handling with respect to navigations.
  http_session->CloseAllConnections(net::ERR_ABORTED, "Clearing auth cache");

  std::move(callback).Run();
}

void NetworkContext::ClearReportingCacheReports(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheReportsCallback callback) {
#if BUILDFLAG(ENABLE_REPORTING)
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
          BuildOriginFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS);
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  std::move(callback).Run();
}

void NetworkContext::ClearReportingCacheClients(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheClientsCallback callback) {
#if BUILDFLAG(ENABLE_REPORTING)
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
          BuildOriginFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  std::move(callback).Run();
}

void NetworkContext::ClearNetworkErrorLogging(
    mojom::ClearDataFilterPtr filter,
    ClearNetworkErrorLoggingCallback callback) {
#if BUILDFLAG(ENABLE_REPORTING)
  net::NetworkErrorLoggingService* logging_service =
      url_request_context_->network_error_logging_service();
  if (logging_service) {
    if (filter) {
      logging_service->RemoveBrowsingData(BuildOriginFilter(std::move(filter)));
    } else {
      logging_service->RemoveAllBrowsingData();
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  std::move(callback).Run();
}

void NetworkContext::SetDocumentReportingEndpoints(
    const base::UnguessableToken& reporting_source,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    const base::flat_map<std::string, std::string>& endpoints) {
#if BUILDFLAG(ENABLE_REPORTING)
  DCHECK(!reporting_source.is_empty());
  DCHECK_EQ(net::IsolationInfo::RequestType::kOther,
            isolation_info.request_type());
  net::ReportingService* reporting_service =
      url_request_context()->reporting_service();
  if (reporting_service) {
    reporting_service->SetDocumentReportingEndpoints(reporting_source, origin,
                                                     isolation_info, endpoints);
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void NetworkContext::SendReportsAndRemoveSource(
    const base::UnguessableToken& reporting_source) {
#if BUILDFLAG(ENABLE_REPORTING)
  DCHECK(!reporting_source.is_empty());
  net::ReportingService* reporting_service =
      url_request_context()->reporting_service();
  if (reporting_service)
    reporting_service->SendReportsAndRemoveSource(reporting_source);
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void NetworkContext::QueueReport(
    const std::string& type,
    const std::string& group,
    const GURL& url,
    const absl::optional<base::UnguessableToken>& reporting_source,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const absl::optional<std::string>& user_agent,
    base::Value::Dict body) {
#if BUILDFLAG(ENABLE_REPORTING)
  // If |reporting_source| is provided, it must not be empty.
  DCHECK(!(reporting_source.has_value() && reporting_source->is_empty()));
  if (require_network_anonymization_key_) {
    DCHECK(!network_anonymization_key.IsEmpty());
  }

  // Get the ReportingService.
  net::URLRequestContext* request_context = url_request_context();
  net::ReportingService* reporting_service =
      request_context->reporting_service();
  // TODO(paulmeyer): Remove this once the network service ships everywhere.
  if (!reporting_service) {
    return;
  }

  std::string reported_user_agent = user_agent.value_or("");
  if (reported_user_agent.empty() &&
      request_context->http_user_agent_settings() != nullptr) {
    reported_user_agent =
        request_context->http_user_agent_settings()->GetUserAgent();
  }

  reporting_service->QueueReport(url, reporting_source,
                                 network_anonymization_key, reported_user_agent,
                                 group, type, std::move(body), 0 /* depth */);
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void NetworkContext::QueueSignedExchangeReport(
    mojom::SignedExchangeReportPtr report,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
#if BUILDFLAG(ENABLE_REPORTING)
  if (require_network_anonymization_key_) {
    DCHECK(!network_anonymization_key.IsEmpty());
  }

  net::NetworkErrorLoggingService* logging_service =
      url_request_context_->network_error_logging_service();
  if (!logging_service)
    return;
  std::string user_agent;
  if (url_request_context_->http_user_agent_settings() != nullptr) {
    user_agent =
        url_request_context_->http_user_agent_settings()->GetUserAgent();
  }
  net::NetworkErrorLoggingService::SignedExchangeReportDetails details;
  details.network_anonymization_key = network_anonymization_key;
  details.success = report->success;
  details.type = std::move(report->type);
  details.outer_url = std::move(report->outer_url);
  details.inner_url = std::move(report->inner_url);
  details.cert_url = std::move(report->cert_url);
  details.referrer = std::move(report->referrer);
  details.server_ip_address = std::move(report->server_ip_address);
  details.protocol = std::move(report->protocol);
  details.method = std::move(report->method);
  details.status_code = report->status_code;
  details.elapsed_time = report->elapsed_time;
  details.user_agent = std::move(user_agent);
  logging_service->QueueSignedExchangeReport(std::move(details));
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

#if BUILDFLAG(ENABLE_REPORTING)
void NetworkContext::AddReportingApiObserver(
    mojo::PendingRemote<network::mojom::ReportingApiObserver> observer) {
  if (url_request_context() && url_request_context()->reporting_service()) {
    if (!is_observing_reporting_service_) {
      is_observing_reporting_service_ = true;
      url_request_context()->reporting_service()->AddReportingCacheObserver(
          this);
      reporting_api_observers_.set_disconnect_handler(
          base::BindRepeating(&NetworkContext::OnReportingObserverDisconnect,
                              weak_factory_.GetWeakPtr()));
    }
    auto id = reporting_api_observers_.Add(std::move(observer));

    auto service_reports =
        url_request_context()->reporting_service()->GetReports();
    for (const auto* service_report : service_reports) {
      reporting_api_observers_.Get(id)->OnReportAdded(*service_report);
    }

    base::flat_map<url::Origin, std::vector<net::ReportingEndpoint>>
        endpoints_by_origin = url_request_context()
                                  ->reporting_service()
                                  ->GetV1ReportingEndpointsByOrigin();
    for (auto const& origin_and_endpoints : endpoints_by_origin) {
      OnEndpointsUpdatedForOrigin(origin_and_endpoints.second);
    }
  }
}

void NetworkContext::OnReportAdded(const net::ReportingReport* service_report) {
  for (const auto& observer : reporting_api_observers_) {
    observer->OnReportAdded(*service_report);
  }
}

void NetworkContext::OnEndpointsUpdatedForOrigin(
    const std::vector<net::ReportingEndpoint>& endpoints) {
  for (const auto& observer : reporting_api_observers_) {
    observer->OnEndpointsUpdatedForOrigin(endpoints);
  }
}

void NetworkContext::OnReportUpdated(
    const net::ReportingReport* service_report) {
  for (const auto& observer : reporting_api_observers_) {
    observer->OnReportUpdated(*service_report);
  }
}

void NetworkContext::OnReportingObserverDisconnect(
    mojo::RemoteSetElementId /*mojo_id*/) {
  if (!reporting_api_observers_.size()) {
    DCHECK(url_request_context());
    DCHECK(url_request_context()->reporting_service());
    url_request_context()->reporting_service()->RemoveReportingCacheObserver(
        this);
    is_observing_reporting_service_ = false;
  }
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void NetworkContext::ClearDomainReliability(
    mojom::ClearDataFilterPtr filter,
    DomainReliabilityClearMode mode,
    ClearDomainReliabilityCallback callback) {
  if (domain_reliability_monitor_) {
    domain_reliability::DomainReliabilityClearMode dr_mode;
    if (mode ==
        mojom::NetworkContext::DomainReliabilityClearMode::CLEAR_CONTEXTS) {
      dr_mode = domain_reliability::CLEAR_CONTEXTS;
    } else {
      dr_mode = domain_reliability::CLEAR_BEACONS;
    }

    domain_reliability_monitor_->ClearBrowsingData(
        dr_mode, BuildOriginFilter(std::move(filter)));
  }
  std::move(callback).Run();
}

void NetworkContext::CloseAllConnections(CloseAllConnectionsCallback callback) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);

  // TODO(mmenke): Use another error code for this, as ERR_ABORTED has somewhat
  // magical handling with respect to navigations.
  http_session->CloseAllConnections(net::ERR_ABORTED,
                                    "Embedder closing all connections");

  std::move(callback).Run();
}

void NetworkContext::CloseIdleConnections(
    CloseIdleConnectionsCallback callback) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);

  http_session->CloseIdleConnections("Embedder closing idle connections");

  std::move(callback).Run();
}

void NetworkContext::SetNetworkConditions(
    const base::UnguessableToken& throttling_profile_id,
    mojom::NetworkConditionsPtr conditions) {
  std::unique_ptr<NetworkConditions> network_conditions;
  if (conditions) {
    network_conditions = std::make_unique<NetworkConditions>(
        conditions->offline, conditions->latency.InMillisecondsF(),
        conditions->download_throughput, conditions->upload_throughput);
  }
  ThrottlingController::SetConditions(throttling_profile_id,
                                      std::move(network_conditions));
}

void NetworkContext::SetAcceptLanguage(const std::string& new_accept_language) {
  // This may only be called on NetworkContexts created with the constructor
  // that calls MakeURLRequestContext().
  DCHECK(user_agent_settings_);
  user_agent_settings_->set_accept_language(new_accept_language);
}

void NetworkContext::SetEnableReferrers(bool enable_referrers) {
  // This may only be called on NetworkContexts created with the constructor
  // that calls MakeURLRequestContext().
  DCHECK(network_delegate_);
  network_delegate_->set_enable_referrers(enable_referrers);
}

#if BUILDFLAG(IS_CHROMEOS)
void NetworkContext::UpdateAdditionalCertificates(
    mojom::AdditionalCertificatesPtr additional_certificates) {
  if (!cert_verifier_with_trust_anchors_) {
    CHECK(g_cert_verifier_for_testing);
    return;
  }
  if (!additional_certificates) {
    cert_verifier_with_trust_anchors_->SetAdditionalCerts(
        net::CertificateList(), net::CertificateList());
    return;
  }

  cert_verifier_with_trust_anchors_->SetAdditionalCerts(
      additional_certificates->trust_anchors,
      additional_certificates->all_certificates);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CT_SUPPORTED)
void NetworkContext::SetCTPolicy(mojom::CTPolicyPtr ct_policy) {
  if (!require_ct_delegate_)
    return;

  require_ct_delegate_->UpdateCTPolicies(
      ct_policy->required_hosts, ct_policy->excluded_hosts,
      ct_policy->excluded_spkis, ct_policy->excluded_legacy_spkis);
}

int NetworkContext::CheckCTComplianceForSignedExchange(
    net::CertVerifyResult& cert_verify_result,
    const net::X509Certificate& certificate,
    const net::HostPortPair& host_port_pair) {
  net::X509Certificate* verified_cert = cert_verify_result.verified_cert.get();

  net::ct::SCTList verified_scts;
  for (const auto& sct_and_status : cert_verify_result.scts) {
    if (sct_and_status.status == net::ct::SCT_STATUS_OK)
      verified_scts.push_back(sct_and_status.sct);
  }
  cert_verify_result.policy_compliance =
      url_request_context_->ct_policy_enforcer()->CheckCompliance(
          verified_cert, verified_scts,
          net::NetLogWithSource::Make(
              url_request_context_->net_log(),
              net::NetLogSourceType::CERT_VERIFIER_JOB));

  // TODO(https://crbug.com/803774): We should determine whether EV & SXG
  // should be a thing (due to the online/offline signing difference)
  if (cert_verify_result.cert_status & net::CERT_STATUS_IS_EV &&
      cert_verify_result.policy_compliance !=
          net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS &&
      cert_verify_result.policy_compliance !=
          net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
    cert_verify_result.cert_status |= net::CERT_STATUS_CT_COMPLIANCE_FAILED;
    cert_verify_result.cert_status &= ~net::CERT_STATUS_IS_EV;
  }

  net::TransportSecurityState::CTRequirementsStatus ct_requirement_status =
      url_request_context_->transport_security_state()->CheckCTRequirements(
          host_port_pair, cert_verify_result.is_issued_by_known_root,
          cert_verify_result.public_key_hashes, verified_cert, &certificate,
          cert_verify_result.scts, cert_verify_result.policy_compliance);

  if (url_request_context_->sct_auditing_delegate()) {
    url_request_context_->sct_auditing_delegate()->MaybeEnqueueReport(
        host_port_pair, verified_cert, cert_verify_result.scts);
  }

  switch (ct_requirement_status) {
    case net::TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
      return net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    case net::TransportSecurityState::CT_REQUIREMENTS_MET:
      return net::OK;
    case net::TransportSecurityState::CT_NOT_REQUIRED:
      // CT is not required if the certificate does not chain to a publicly
      // trusted root certificate.
      if (!cert_verify_result.is_issued_by_known_root)
        return net::OK;
      // For old certificates (issued before 2018-05-01),
      // CheckCTRequirements() may return CT_NOT_REQUIRED, so we check the
      // compliance status here.
      // TODO(https://crbug.com/851778): Remove this condition once we require
      // signing certificates to have CanSignHttpExchanges extension, because
      // such certificates should be naturally after 2018-05-01.
      if (cert_verify_result.policy_compliance ==
              net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
          cert_verify_result.policy_compliance ==
              net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
        return net::OK;
      }
      // Require CT compliance, by overriding CT_NOT_REQUIRED and treat it as
      // ERR_CERTIFICATE_TRANSPARENCY_REQUIRED.
      return net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
  }
}

void NetworkContext::MaybeEnqueueSCTReport(
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  sct_auditing_handler()->MaybeEnqueueReport(host_port_pair,
                                             validated_certificate_chain,
                                             signed_certificate_timestamps);
}

void NetworkContext::SetCTLogListAlwaysTimelyForTesting() {
  if (!ct_policy_enforcer_)
    return;
  ct_policy_enforcer_->SetCTLogListAlwaysTimelyForTesting(true);
}

void NetworkContext::SetSCTAuditingMode(mojom::SCTAuditingMode mode) {
  sct_auditing_handler()->SetMode(mode);
}

void NetworkContext::OnCTLogListUpdated(
    const std::vector<network::mojom::CTLogInfoPtr>& log_list,
    base::Time update_time) {
  if (!ct_policy_enforcer_)
    return;
  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::vector<std::string> operated_by_google_logs;
  std::map<std::string, certificate_transparency::OperatorHistoryEntry>
      log_operator_history;
  GetCTPolicyConfigForCTLogInfo(log_list, &disqualified_logs,
                                &operated_by_google_logs,
                                &log_operator_history);
  ct_policy_enforcer_->UpdateCTLogList(
      update_time, std::move(disqualified_logs),
      std::move(operated_by_google_logs), std::move(log_operator_history));
}

void NetworkContext::CanSendSCTAuditingReport(
    base::OnceCallback<void(bool)> callback) {
  // If the NetworkContextClient hasn't been set yet or has disconnected for
  // some reason, just return `false`. (One case where this could occur is when
  // restarting SCTAuditingReporter instances loaded form disk at startup -- see
  // crbug.com/1347180 for more details on that case.)
  if (!client_) {
    std::move(callback).Run(false);
    return;
  }
  client_->OnCanSendSCTAuditingReport(std::move(callback));
}

void NetworkContext::OnNewSCTAuditingReportSent() {
  client_->OnNewSCTAuditingReportSent();
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

void NetworkContext::CreateUDPSocket(
    mojo::PendingReceiver<mojom::UDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener) {
  socket_factory_->CreateUDPSocket(std::move(receiver), std::move(listener));
}

void NetworkContext::CreateRestrictedUDPSocket(
    const net::IPEndPoint& addr,
    mojom::RestrictedUDPSocketMode mode,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::RestrictedUDPSocketParamsPtr params,
    mojo::PendingReceiver<mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener,
    CreateRestrictedUDPSocketCallback callback) {
  // SimpleHostResolver is transitively owned by |this|.
  socket_factory_->CreateRestrictedUDPSocket(
      addr, mode, traffic_annotation, std::move(params), std::move(receiver),
      std::move(listener), SimpleHostResolver::Create(this),
      std::move(callback));
}

void NetworkContext::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    mojom::TCPServerSocketOptionsPtr options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    CreateTCPServerSocketCallback callback) {
  socket_factory_->CreateTCPServerSocket(
      local_addr, std::move(options),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(receiver), std::move(callback));
}

void NetworkContext::CreateTCPConnectedSocket(
    const absl::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    CreateTCPConnectedSocketCallback callback) {
  socket_factory_->CreateTCPConnectedSocket(
      local_addr, remote_addr_list, std::move(tcp_connected_socket_options),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(receiver), std::move(observer), std::move(callback));
}

void NetworkContext::CreateTCPBoundSocket(
    const net::IPEndPoint& local_addr,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPBoundSocket> receiver,
    CreateTCPBoundSocketCallback callback) {
  socket_factory_->CreateTCPBoundSocket(
      local_addr,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(receiver), std::move(callback));
}

void NetworkContext::CreateProxyResolvingSocketFactory(
    mojo::PendingReceiver<mojom::ProxyResolvingSocketFactory> receiver) {
  proxy_resolving_socket_factories_.Add(
      std::make_unique<ProxyResolvingSocketFactoryMojo>(url_request_context()),
      std::move(receiver));
}

void NetworkContext::LookUpProxyForURL(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonyization_key,
    mojo::PendingRemote<mojom::ProxyLookupClient> proxy_lookup_client) {
  DCHECK(proxy_lookup_client);
  std::unique_ptr<ProxyLookupRequest> proxy_lookup_request(
      std::make_unique<ProxyLookupRequest>(std::move(proxy_lookup_client), this,
                                           network_anonyization_key));
  ProxyLookupRequest* proxy_lookup_request_ptr = proxy_lookup_request.get();
  proxy_lookup_requests_.insert(std::move(proxy_lookup_request));
  proxy_lookup_request_ptr->Start(url);
}

void NetworkContext::ForceReloadProxyConfig(
    ForceReloadProxyConfigCallback callback) {
  net::ConfiguredProxyResolutionService* configured_proxy_resolution_service =
      nullptr;
  if (url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(
              &configured_proxy_resolution_service)) {
    configured_proxy_resolution_service->ForceReloadProxyConfig();
  } else {
    LOG(WARNING)
        << "NetworkContext::ForceReloadProxyConfig() had no effect, as the "
           "underlying ProxyResolutionService does not support that concept.";
  }
  std::move(callback).Run();
}

void NetworkContext::ClearBadProxiesCache(
    ClearBadProxiesCacheCallback callback) {
  url_request_context()->proxy_resolution_service()->ClearBadProxiesCache();
  std::move(callback).Run();
}

void NetworkContext::CreateWebSocket(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::SiteForCookies& site_for_cookies,
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
    const absl::optional<base::UnguessableToken>& throttling_profile_id) {
#if BUILDFLAG(ENABLE_WEBSOCKETS)
  if (!websocket_factory_)
    websocket_factory_ = std::make_unique<WebSocketFactory>(this);

  DCHECK_GE(process_id, 0);

  websocket_factory_->CreateWebSocket(
      url, requested_protocols, site_for_cookies, isolation_info,
      std::move(additional_headers), process_id, origin, options,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(handshake_client), std::move(url_loader_network_observer),
      std::move(auth_handler), std::move(header_client), throttling_profile_id);
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
}

void NetworkContext::CreateWebTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkAnonymizationKey& key,
    std::vector<mojom::WebTransportCertificateFingerprintPtr> fingerprints,
    mojo::PendingRemote<mojom::WebTransportHandshakeClient>
        pending_handshake_client) {
  web_transports_.insert(
      std::make_unique<WebTransport>(url, origin, key, fingerprints, this,
                                     std::move(pending_handshake_client)));
}

void NetworkContext::CreateNetLogExporter(
    mojo::PendingReceiver<mojom::NetLogExporter> receiver) {
  net_log_exporter_receivers_.Add(std::make_unique<NetLogExporter>(this),
                                  std::move(receiver));
}

void NetworkContext::ResolveHost(
    mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<mojom::ResolveHostClient> response_client) {
  if (!internal_host_resolver_) {
    internal_host_resolver_ = std::make_unique<HostResolver>(
        url_request_context_->host_resolver(), url_request_context_->net_log());
  }
  internal_host_resolver_->ResolveHost(
      std::move(host), network_anonymization_key,
      std::move(optional_parameters), std::move(response_client));
}

void NetworkContext::CreateHostResolver(
    const absl::optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<mojom::HostResolver> receiver) {
  net::HostResolver* internal_resolver = url_request_context_->host_resolver();
  std::unique_ptr<net::HostResolver> private_internal_resolver;

  if (config_overrides &&
      config_overrides.value() != net::DnsConfigOverrides()) {
    // If custom configuration is needed, create a separate internal resolver
    // with the specified configuration overrides. Because we are using a non-
    // standard resolver, disable the cache.
    //
    // TODO(crbug.com/846423): Consider allowing per-resolve overrides, so the
    // same net::HostResolver with the same scheduler and cache can be used with
    // different overrides.  But since this is only used for special cases for
    // now, much easier to create entirely separate net::HostResolver instances.
    net::HostResolver::ManagerOptions options;
    options.insecure_dns_client_enabled = true;
    // Assume additional types are unnecessary for these special cases.
    options.additional_types_via_insecure_dns_enabled = false;
    options.dns_config_overrides = config_overrides.value();
    private_internal_resolver =
        network_service_->host_resolver_factory()->CreateStandaloneResolver(
            url_request_context_->net_log(), std::move(options),
            "" /* host_mapping_rules */, false /* enable_caching */);
    private_internal_resolver->SetRequestContext(url_request_context_);
    internal_resolver = private_internal_resolver.get();
  }

  host_resolvers_.emplace(
      std::make_unique<HostResolver>(
          std::move(receiver),
          base::BindOnce(&NetworkContext::OnHostResolverShutdown,
                         base::Unretained(this)),
          internal_resolver, url_request_context_->net_log()),
      std::move(private_internal_resolver));
}

void NetworkContext::VerifyCertForSignedExchange(
    const scoped_refptr<net::X509Certificate>& certificate,
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::string& ocsp_result,
    const std::string& sct_list,
    VerifyCertForSignedExchangeCallback callback) {
  if (require_network_anonymization_key_) {
    DCHECK(!network_anonymization_key.IsEmpty());
  }

  uint64_t cert_verify_id = ++next_cert_verify_id_;
  CHECK_NE(0u, next_cert_verify_id_);  // The request ID should not wrap around.
  auto pending_cert_verify = std::make_unique<PendingCertVerify>();
  pending_cert_verify->callback = std::move(callback);
  pending_cert_verify->result = std::make_unique<net::CertVerifyResult>();
  pending_cert_verify->certificate = certificate;
  pending_cert_verify->url = url;
  pending_cert_verify->network_anonymization_key = network_anonymization_key;
  pending_cert_verify->ocsp_result = ocsp_result;
  pending_cert_verify->sct_list = sct_list;
  net::CertVerifier* cert_verifier =
      g_cert_verifier_for_testing ? g_cert_verifier_for_testing
                                  : url_request_context_->cert_verifier();
  int result = cert_verifier->Verify(
      net::CertVerifier::RequestParams(certificate, url.host(),
                                       0 /* cert_verify_flags */, ocsp_result,
                                       sct_list),
      pending_cert_verify->result.get(),
      base::BindOnce(&NetworkContext::OnVerifyCertForSignedExchangeComplete,
                     base::Unretained(this), cert_verify_id),
      &pending_cert_verify->request,
      net::NetLogWithSource::Make(url_request_context_->net_log(),
                                  net::NetLogSourceType::CERT_VERIFIER_JOB));
  cert_verifier_requests_[cert_verify_id] = std::move(pending_cert_verify);

  if (result != net::ERR_IO_PENDING)
    OnVerifyCertForSignedExchangeComplete(cert_verify_id, result);
}

void NetworkContext::NotifyExternalCacheHit(const GURL& url,
                                            const std::string& http_method,
                                            const net::NetworkIsolationKey& key,
                                            bool is_subframe_document_resource,
                                            bool include_credentials) {
  net::HttpCache* cache =
      url_request_context_->http_transaction_factory()->GetCache();
  if (!cache)
    return;
  cache->OnExternalCacheHit(url, http_method, key,
                            is_subframe_document_resource, include_credentials);
}

void NetworkContext::SetCorsOriginAccessListsForOrigin(
    const url::Origin& source_origin,
    std::vector<mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<mojom::CorsOriginPatternPtr> block_patterns,
    SetCorsOriginAccessListsForOriginCallback callback) {
  cors_origin_access_list_.SetAllowListForOrigin(source_origin, allow_patterns);
  cors_origin_access_list_.SetBlockListForOrigin(source_origin, block_patterns);
  std::move(callback).Run();
}

void NetworkContext::AddHSTS(const std::string& host,
                             base::Time expiry,
                             bool include_subdomains,
                             AddHSTSCallback callback) {
  net::TransportSecurityState* state =
      url_request_context_->transport_security_state();
  state->AddHSTS(host, expiry, include_subdomains);
  std::move(callback).Run();
}

void NetworkContext::IsHSTSActiveForHost(const std::string& host,
                                         IsHSTSActiveForHostCallback callback) {
  net::TransportSecurityState* security_state =
      url_request_context_->transport_security_state();

  if (!security_state) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(security_state->ShouldUpgradeToSSL(host));
}

void NetworkContext::GetHSTSState(const std::string& domain,
                                  GetHSTSStateCallback callback) {
  base::Value::Dict result;

  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        url_request_context()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::STSState static_sts_state;
      net::TransportSecurityState::PKPState static_pkp_state;
      bool found_sts_static = transport_security_state->GetStaticSTSState(
          domain, &static_sts_state);
      bool found_pkp_static = transport_security_state->GetStaticPKPState(
          domain, &static_pkp_state);
      if (found_sts_static || found_pkp_static) {
        result.Set("static_upgrade_mode",
                   static_cast<int>(static_sts_state.upgrade_mode));
        result.Set("static_sts_include_subdomains",
                   static_sts_state.include_subdomains);
        result.Set("static_sts_observed",
                   static_sts_state.last_observed.ToDoubleT());
        result.Set("static_sts_expiry", static_sts_state.expiry.ToDoubleT());
        result.Set("static_pkp_include_subdomains",
                   static_pkp_state.include_subdomains);
        result.Set("static_pkp_observed",
                   static_pkp_state.last_observed.ToDoubleT());
        result.Set("static_pkp_expiry", static_pkp_state.expiry.ToDoubleT());
        result.Set("static_spki_hashes",
                   HashesToBase64String(static_pkp_state.spki_hashes));
        result.Set("static_sts_domain", static_sts_state.domain);
        result.Set("static_pkp_domain", static_pkp_state.domain);
      }

      net::TransportSecurityState::STSState dynamic_sts_state;
      net::TransportSecurityState::PKPState dynamic_pkp_state;
      bool found_sts_dynamic = transport_security_state->GetDynamicSTSState(
          domain, &dynamic_sts_state);

      bool found_pkp_dynamic = transport_security_state->GetDynamicPKPState(
          domain, &dynamic_pkp_state);
      if (found_sts_dynamic) {
        result.Set("dynamic_upgrade_mode",
                   static_cast<int>(dynamic_sts_state.upgrade_mode));
        result.Set("dynamic_sts_include_subdomains",
                   dynamic_sts_state.include_subdomains);
        result.Set("dynamic_sts_observed",
                   dynamic_sts_state.last_observed.ToDoubleT());
        result.Set("dynamic_sts_expiry", dynamic_sts_state.expiry.ToDoubleT());
        result.Set("dynamic_sts_domain", dynamic_sts_state.domain);
      }

      if (found_pkp_dynamic) {
        result.Set("dynamic_pkp_include_subdomains",
                   dynamic_pkp_state.include_subdomains);
        result.Set("dynamic_pkp_observed",
                   dynamic_pkp_state.last_observed.ToDoubleT());
        result.Set("dynamic_pkp_expiry", dynamic_pkp_state.expiry.ToDoubleT());
        result.Set("dynamic_spki_hashes",
                   HashesToBase64String(dynamic_pkp_state.spki_hashes));
        result.Set("dynamic_pkp_domain", dynamic_pkp_state.domain);
      }

      result.Set("result", found_sts_static || found_pkp_static ||
                               found_sts_dynamic || found_pkp_dynamic);
    } else {
      result.Set("error", "no TransportSecurityState active");
    }
  } else {
    result.Set("error", "non-ASCII domain name");
  }

  std::move(callback).Run(std::move(result));
}

void NetworkContext::DeleteDynamicDataForHost(
    const std::string& host,
    DeleteDynamicDataForHostCallback callback) {
  net::TransportSecurityState* transport_security_state =
      url_request_context()->transport_security_state();
  if (!transport_security_state) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      transport_security_state->DeleteDynamicDataForHost(host));
}

void NetworkContext::EnableStaticKeyPinningForTesting(
    EnableStaticKeyPinningForTestingCallback callback) {
  net::TransportSecurityState* state =
      url_request_context_->transport_security_state();
  state->EnableStaticPinsForTesting();
  state->SetPinningListAlwaysTimelyForTesting(true);
  std::move(callback).Run();
}

void NetworkContext::VerifyCertificateForTesting(
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    VerifyCertificateForTestingCallback callback) {
  net::CertVerifier* cert_verifier = url_request_context_->cert_verifier();

  auto state = std::make_unique<TestVerifyCertState>();
  auto* request = &state->request;
  auto* result = &state->result;

  cert_verifier->Verify(
      net::CertVerifier::RequestParams(certificate.get(), hostname, 0,
                                       ocsp_response, sct_list),
      result,
      base::BindOnce(TestVerifyCertCallback, std::move(state),
                     std::move(callback)),
      request, net::NetLogWithSource());
}

void NetworkContext::VerifyIpProtectionConfigGetterForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  // This method assumes that the proxy delegate and auth token cache have been
  // initialized.
  CHECK(proxy_delegate_);

  auto* ipp_config_cache_impl = static_cast<IpProtectionConfigCacheImpl*>(
      proxy_delegate_->GetIpProtectionConfigCacheForTesting());  // IN-TEST
  CHECK(ipp_config_cache_impl);

  ipp_config_cache_impl->FillCacheForTesting(base::BindOnce(  // IN-TEST
      &NetworkContext::OnIpProtectionConfigAvailableForTesting,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkContext::OnIpProtectionConfigAvailableForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  auto* ipp_config_cache =
      proxy_delegate_->GetIpProtectionConfigCacheForTesting();  // IN-TEST

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> result =
      ipp_config_cache->GetAuthToken();
  CHECK(result.has_value());
  std::move(callback).Run(std::move(result).value());
}

void NetworkContext::PreconnectSockets(
    uint32_t num_streams,
    const GURL& original_url,
    bool allow_credentials,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK(!require_network_anonymization_key_ ||
         !network_anonymization_key.IsEmpty());

  GURL url = GetHSTSRedirect(original_url);

  // |PreconnectSockets| may receive arguments from the renderer, which is not
  // guaranteed to validate them.
  if (num_streams == 0)
    return;

  std::string user_agent;
  if (url_request_context_->http_user_agent_settings()) {
    user_agent =
        url_request_context_->http_user_agent_settings()->GetUserAgent();
  }
  net::HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = net::HttpRequestHeaders::kGetMethod;
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                       user_agent);

  if (allow_credentials) {
    request_info.load_flags = net::LOAD_NORMAL;
    request_info.privacy_mode = net::PRIVACY_MODE_DISABLED;
  } else {
    request_info.load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
    request_info.privacy_mode = net::PRIVACY_MODE_ENABLED;
  }
  request_info.network_anonymization_key = network_anonymization_key;

  net::HttpTransactionFactory* factory =
      url_request_context_->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();
  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  http_stream_factory->PreconnectStreams(
      base::saturated_cast<int32_t>(num_streams), request_info);
}

#if BUILDFLAG(IS_P2P_ENABLED)
void NetworkContext::CreateP2PSocketManager(
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client,
    mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
        trusted_socket_manager,
    mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver) {
  std::unique_ptr<P2PSocketManager> socket_manager =
      std::make_unique<P2PSocketManager>(
          network_anonymization_key, std::move(client),
          std::move(trusted_socket_manager), std::move(socket_manager_receiver),
          base::BindRepeating(&NetworkContext::DestroySocketManager,
                              base::Unretained(this)),
          url_request_context_);
  socket_managers_[socket_manager.get()] = std::move(socket_manager);
}
#endif  // BUILDFLAG(IS_P2P_ENABLED)

void NetworkContext::CreateMdnsResponder(
    mojo::PendingReceiver<mojom::MdnsResponder> responder_receiver) {
#if BUILDFLAG(ENABLE_MDNS)
  if (!mdns_responder_manager_)
    mdns_responder_manager_ = std::make_unique<MdnsResponderManager>();

  mdns_responder_manager_->CreateMdnsResponder(std::move(responder_receiver));
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_MDNS)
}

void NetworkContext::AddDomainReliabilityContextForTesting(
    const url::Origin& origin,
    const GURL& upload_url,
    AddDomainReliabilityContextForTestingCallback callback) {
  auto config = std::make_unique<domain_reliability::DomainReliabilityConfig>();
  config->origin = origin;
  config->include_subdomains = false;
  config->collectors.push_back(std::make_unique<GURL>(upload_url));
  config->success_sample_rate = 1.0;
  config->failure_sample_rate = 1.0;
  domain_reliability_monitor_->AddContextForTesting(std::move(config));
  std::move(callback).Run();
}

void NetworkContext::ForceDomainReliabilityUploadsForTesting(
    ForceDomainReliabilityUploadsForTestingCallback callback) {
  domain_reliability_monitor_->ForceUploadsForTesting();
  std::move(callback).Run();
}

void NetworkContext::SetSplitAuthCacheByNetworkAnonymizationKey(
    bool split_auth_cache_by_network_anonymization_key) {
  url_request_context_->http_transaction_factory()
      ->GetSession()
      ->http_auth_cache()
      ->SetKeyServerEntriesByNetworkAnonymizationKey(
          split_auth_cache_by_network_anonymization_key);
}

void NetworkContext::SaveHttpAuthCacheProxyEntries(
    SaveHttpAuthCacheProxyEntriesCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  base::UnguessableToken cache_key =
      network_service_->http_auth_cache_copier()->SaveHttpAuthCache(
          *http_auth_cache);
  std::move(callback).Run(cache_key);
}

void NetworkContext::LoadHttpAuthCacheProxyEntries(
    const base::UnguessableToken& cache_key,
    LoadHttpAuthCacheProxyEntriesCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  network_service_->http_auth_cache_copier()->LoadHttpAuthCache(
      cache_key, http_auth_cache);
  std::move(callback).Run();
}

void NetworkContext::AddAuthCacheEntry(
    const net::AuthChallengeInfo& challenge,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const net::AuthCredentials& credentials,
    AddAuthCacheEntryCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  http_auth_cache->Add(challenge.challenger,
                       challenge.is_proxy ? net::HttpAuth::AUTH_PROXY
                                          : net::HttpAuth::AUTH_SERVER,
                       challenge.realm,
                       net::HttpAuth::StringToScheme(challenge.scheme),
                       network_anonymization_key, challenge.challenge,
                       credentials, challenge.path);
  std::move(callback).Run();
}

void NetworkContext::SetCorsNonWildcardRequestHeadersSupport(bool value) {
  if (!base::FeatureList::IsEnabled(
          features::kCorsNonWildcardRequestHeadersSupport)) {
    return;
  }
  cors_non_wildcard_request_headers_support_ =
      cors::NonWildcardRequestHeadersSupport(value);
}

void NetworkContext::LookupServerBasicAuthCredentials(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    LookupServerBasicAuthCredentialsCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  net::HttpAuthCache::Entry* entry = http_auth_cache->LookupByPath(
      url::SchemeHostPort(url), net::HttpAuth::AUTH_SERVER,
      network_anonymization_key, url.path());
  if (entry && entry->scheme() == net::HttpAuth::AUTH_SCHEME_BASIC)
    std::move(callback).Run(entry->credentials());
  else
    std::move(callback).Run(absl::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void NetworkContext::LookupProxyAuthCredentials(
    const net::ProxyServer& proxy_server,
    const std::string& auth_scheme,
    const std::string& realm,
    LookupProxyAuthCredentialsCallback callback) {
  net::HttpAuth::Scheme net_scheme =
      net::HttpAuth::StringToScheme(base::ToLowerASCII(auth_scheme));
  if (net_scheme == net::HttpAuth::Scheme::AUTH_SCHEME_MAX) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  // TODO(https://crbug.com/1103768): Mapping proxy addresses to URLs is a
  // lossy conversion, shouldn't do this.
  const char* scheme =
      proxy_server.is_secure_http_like() ? "https://" : "http://";
  url::SchemeHostPort scheme_host_port(
      GURL(scheme + proxy_server.host_port_pair().ToString()));
  if (!scheme_host_port.IsValid()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  //  Unlike server credentials, proxy credentials are not keyed on
  //  NetworkAnonymizationKey.
  net::HttpAuthCache::Entry* entry = http_auth_cache->Lookup(
      scheme_host_port, net::HttpAuth::AUTH_PROXY, realm, net_scheme,
      net::NetworkAnonymizationKey());
  if (entry)
    std::move(callback).Run(entry->credentials());
  else
    std::move(callback).Run(absl::nullopt);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const net::HttpAuthPreferences* NetworkContext::GetHttpAuthPreferences() const {
  return &http_auth_merged_preferences_;
}

NetworkServiceMemoryCache* NetworkContext::GetMemoryCache() {
  return memory_cache_.get();
}

size_t NetworkContext::NumOpenWebTransports() const {
  return base::ranges::count(web_transports_, false, &WebTransport::torn_down);
}

void NetworkContext::OnHttpAuthDynamicParamsChanged(
    const mojom::HttpAuthDynamicParams*
        http_auth_dynamic_network_service_params) {
  http_auth_merged_preferences_.SetServerAllowlist(
      http_auth_dynamic_network_service_params->server_allowlist);
  http_auth_merged_preferences_.SetDelegateAllowlist(
      http_auth_dynamic_network_service_params->delegate_allowlist);
  http_auth_merged_preferences_.set_delegate_by_kdc_policy(
      http_auth_dynamic_network_service_params->delegate_by_kdc_policy);
  http_auth_merged_preferences_.set_negotiate_disable_cname_lookup(
      http_auth_dynamic_network_service_params->negotiate_disable_cname_lookup);
  http_auth_merged_preferences_.set_negotiate_enable_port(
      http_auth_dynamic_network_service_params->enable_negotiate_port);
  http_auth_merged_preferences_.set_basic_over_http_enabled(
      http_auth_dynamic_network_service_params->basic_over_http_enabled);
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  http_auth_merged_preferences_.set_ntlm_v2_enabled(
      http_auth_dynamic_network_service_params->ntlm_v2_enabled);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
  http_auth_merged_preferences_.set_auth_android_negotiate_account_type(
      http_auth_dynamic_network_service_params->android_negotiate_account_type);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  http_auth_merged_preferences_.set_allow_gssapi_library_load(
      http_auth_dynamic_network_service_params->allow_gssapi_library_load);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  if (http_auth_dynamic_network_service_params->allowed_schemes.has_value()) {
    http_auth_merged_preferences_.set_allowed_schemes(std::set<std::string>(
        http_auth_dynamic_network_service_params->allowed_schemes->begin(),
        http_auth_dynamic_network_service_params->allowed_schemes->end()));
  } else {
    http_auth_merged_preferences_.set_allowed_schemes(absl::nullopt);
  }

  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFilters(url_matcher_.get(),
                                     http_auth_dynamic_network_service_params
                                         ->patterns_allowed_to_use_all_schemes);
  http_auth_merged_preferences_.set_http_auth_scheme_filter(
      base::BindRepeating(&NetworkContext::IsAllowedToUseAllHttpAuthSchemes,
                          base::Unretained(this)));
}

URLRequestContextOwner NetworkContext::MakeURLRequestContext(
    mojo::PendingRemote<mojom::URLLoaderFactory>
        url_loader_factory_for_cert_net_fetcher,
    scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store,
    OnURLRequestContextBuilderConfiguredCallback
        on_url_request_context_builder_configured) {
  URLRequestContextBuilderMojo builder;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  std::unique_ptr<net::CertVerifier> cert_verifier;
  if (g_cert_verifier_for_testing) {
    cert_verifier = std::make_unique<WrappedTestingCertVerifier>();
  } else {
    DCHECK(params_->cert_verifier_params);
    // base::Unretained() is safe below because |this| will own
    // |cert_verifier|.
    // TODO(https://crbug.com/1085233): this cert verifier should deal with
    // disconnections if the CertVerifierService is run outside of the browser
    // process.
    cert_verifier = std::make_unique<cert_verifier::MojoCertVerifier>(
        std::move(params_->cert_verifier_params->cert_verifier_service),
        std::move(params_->cert_verifier_params
                      ->cert_verifier_service_client_receiver),
        std::move(url_loader_factory_for_cert_net_fetcher),
        base::BindRepeating(
            &NetworkContext::CreateURLLoaderFactoryForCertNetFetcher,
            base::Unretained(this)));

#if BUILDFLAG(IS_CT_SUPPORTED)
    std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
    for (const auto& log : network_service_->log_list()) {
      scoped_refptr<const net::CTLogVerifier> log_verifier =
          net::CTLogVerifier::Create(log->public_key, log->name);
      if (!log_verifier) {
        // TODO: Signal bad configuration (such as bad key).
        continue;
      }
      ct_logs.push_back(std::move(log_verifier));
    }
    auto ct_verifier = std::make_unique<net::MultiLogCTVerifier>(
        network_service_->ct_log_list_distributor());
    ct_verifier->SetLogs(ct_logs);
    cert_verifier = std::make_unique<net::CertAndCTVerifier>(
        std::move(cert_verifier), std::move(ct_verifier));
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

    // Whether the cert verifier is remote or in-process, we should wrap it in
    // caching and coalescing layers to avoid extra verifications and IPCs.
    cert_verifier = std::make_unique<net::CachingCertVerifier>(
        std::make_unique<net::CoalescingCertVerifier>(
            std::move(cert_verifier)));

#if BUILDFLAG(IS_CHROMEOS)
    cert_verifier_with_trust_anchors_ =
        new CertVerifierWithTrustAnchors(base::BindRepeating(
            &NetworkContext::TrustAnchorUsed, base::Unretained(this)));
    UpdateAdditionalCertificates(
        std::move(params_->initial_additional_certificates));
    cert_verifier_with_trust_anchors_->InitializeOnIOThread(
        std::move(cert_verifier));
    cert_verifier = base::WrapUnique(cert_verifier_with_trust_anchors_.get());
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  builder.SetCertVerifier(IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
      *command_line, nullptr, std::move(cert_verifier)));

#if BUILDFLAG(IS_CT_SUPPORTED)
  if (params_->enforce_chrome_ct_policy) {
    std::vector<std::pair<std::string, base::Time>> disqualified_logs;
    std::vector<std::string> operated_by_google_logs;
    std::map<std::string, certificate_transparency::OperatorHistoryEntry>
        log_operator_history;
    GetCTPolicyConfigForCTLogInfo(network_service_->log_list(),
                                  &disqualified_logs, &operated_by_google_logs,
                                  &log_operator_history);
    auto ct_policy_enforcer =
        std::make_unique<certificate_transparency::ChromeCTPolicyEnforcer>(
            network_service_->ct_log_list_update_time(),
            std::move(disqualified_logs), std::move(operated_by_google_logs),
            std::move(log_operator_history));
    ct_policy_enforcer_ = ct_policy_enforcer.get();
    builder.set_ct_policy_enforcer(std::move(ct_policy_enforcer));
  }

  builder.set_sct_auditing_delegate(
      std::make_unique<SCTAuditingDelegate>(weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  std::unique_ptr<NetworkServiceNetworkDelegate> network_delegate =
      std::make_unique<NetworkServiceNetworkDelegate>(
          params_->enable_referrers,
          params_->validate_referrer_policy_on_initial_request,
          std::move(params_->proxy_error_client), this);
  network_delegate_ = network_delegate.get();
  builder.set_network_delegate(std::move(network_delegate));

  if (params_->initial_custom_proxy_config ||
      params_->custom_proxy_config_client_receiver) {
    std::unique_ptr<NetworkServiceProxyDelegate> proxy_delegate =
        std::make_unique<NetworkServiceProxyDelegate>(
            std::move(params_->initial_custom_proxy_config),
            std::move(params_->custom_proxy_config_client_receiver),
            std::move(params_->custom_proxy_connection_observer_remote),
            network_service_->network_service_proxy_allow_list());
    proxy_delegate_ = proxy_delegate.get();
    builder.set_proxy_delegate(std::move(proxy_delegate));
  }

  net::NetLog* net_log = nullptr;
  if (network_service_) {
    net_log = network_service_->net_log();
    builder.set_net_log(net_log);
    builder.set_host_resolver_manager(
        network_service_->host_resolver_manager());
    builder.set_host_resolver_factory(
        network_service_->host_resolver_factory());
    builder.SetHttpAuthHandlerFactory(
        network_service_->CreateHttpAuthHandlerFactory(this));
    builder.set_network_quality_estimator(
        network_service_->network_quality_estimator());
  }

  if (session_cleanup_cookie_store) {
    std::unique_ptr<net::CookieMonster> cookie_store =
        std::make_unique<net::CookieMonster>(session_cleanup_cookie_store.get(),
                                             net_log);
    if (params_->persist_session_cookies)
      cookie_store->SetPersistSessionCookies(true);

    builder.SetCookieStore(std::move(cookie_store));
  }

  if (base::FeatureList::IsEnabled(features::kPrivateStateTokens) ||
      base::FeatureList::IsEnabled(features::kFledgePst)) {
    trust_token_store_ = std::make_unique<PendingTrustTokenStore>();

    base::FilePath trust_token_path;
    if (GetFullDataFilePath(
            params_->file_paths,
            &network::mojom::NetworkContextFilePaths::trust_token_database_name,
            trust_token_path)) {
      SQLiteTrustTokenPersister::CreateForFilePath(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), kTrustTokenDatabaseTaskPriority,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          trust_token_path, kTrustTokenWriteBufferingWindow,
          base::BindOnce(&NetworkContext::FinishConstructingTrustTokenStore,
                         weak_factory_.GetWeakPtr()));
    } else {
      trust_token_store_->OnStoreReady(std::make_unique<TrustTokenStore>(
          std::make_unique<InMemoryTrustTokenPersister>(),
          std::make_unique<ExpiryInspectingRecordExpiryDelegate>(
              network_service()->trust_token_key_commitments())));
    }
  }

  std::unique_ptr<net::StaticHttpUserAgentSettings> user_agent_settings =
      std::make_unique<net::StaticHttpUserAgentSettings>(
          params_->accept_language, params_->user_agent);
  // Borrow an alias for future use before giving the builder ownership.
  user_agent_settings_ = user_agent_settings.get();
  builder.set_http_user_agent_settings(std::move(user_agent_settings));

  builder.set_enable_brotli(params_->enable_brotli);
  builder.set_enable_zstd(params_->enable_zstd);

  if (params_->proxy_resolver_factory) {
    builder.SetMojoProxyResolverFactory(
        std::move(params_->proxy_resolver_factory));
  }

#if BUILDFLAG(IS_WIN)
  if (params_->windows_system_proxy_resolver) {
    builder.SetMojoWindowsSystemProxyResolver(
        std::move(params_->windows_system_proxy_resolver));
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (params_->dhcp_wpad_url_client) {
    builder.SetDhcpWpadUrlClient(std::move(params_->dhcp_wpad_url_client));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!params_->http_cache_enabled) {
    builder.DisableHttpCache();
  } else {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.max_size = params_->http_cache_max_size;
    // Checking both to see if there are any file paths at all, and if there is
    // specifically an http_cache_directory filepath in order to avoid a
    // potential nullptr dereference if we just checked that
    // `params_->file_paths->http_cache_directory' existed.
    if (!params_->file_paths || !params_->file_paths->http_cache_directory) {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    } else {
      cache_params.path = params_->file_paths->http_cache_directory->path();
      cache_params.type = network_session_configurator::ChooseCacheType();
      if (params_->http_cache_file_operations_factory) {
        cache_params.file_operations_factory =
            base::MakeRefCounted<MojoBackendFileOperationsFactory>(
                std::move(params_->http_cache_file_operations_factory));
      }
    }
    cache_params.reset_cache = params_->reset_http_cache_backend;

#if BUILDFLAG(IS_ANDROID)
    app_status_listeners_.push_back(
        std::make_unique<NetworkContextApplicationStatusListener>());
    cache_params.app_status_listener = app_status_listeners_.rbegin()->get();
#endif  // BUILDFLAG(IS_ANDROID)
    builder.EnableHttpCache(cache_params);
  }

  std::unique_ptr<SSLConfigServiceMojo> ssl_config_service =
      std::make_unique<SSLConfigServiceMojo>(
          std::move(params_->initial_ssl_config),
          std::move(params_->ssl_config_client_receiver));
  SSLConfigServiceMojo* ssl_config_service_raw = ssl_config_service.get();
  builder.set_ssl_config_service(std::move(ssl_config_service));

  if (!params_->initial_proxy_config &&
      !params_->proxy_config_client_receiver.is_valid()) {
    params_->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
  }
  builder.set_proxy_config_service(std::make_unique<ProxyConfigServiceMojo>(
      std::move(params_->proxy_config_client_receiver),
      std::move(params_->initial_proxy_config),
      std::move(params_->proxy_config_poller_client)));
  builder.set_pac_quick_check_enabled(params_->pac_quick_check_enabled);

  std::unique_ptr<PrefService> pref_service;

  base::FilePath http_server_properties_file_name;
  if (GetFullDataFilePath(params_->file_paths,
                          &network::mojom::NetworkContextFilePaths::
                              http_server_properties_file_name,
                          http_server_properties_file_name)) {
    scoped_refptr<JsonPrefStore> json_pref_store(new JsonPrefStore(
        http_server_properties_file_name, nullptr,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
             base::TaskPriority::BEST_EFFORT})));
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(json_pref_store);
    pref_service_factory.set_async(true);
    scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
    HttpServerPropertiesPrefDelegate::RegisterPrefs(pref_registry.get());
    NetworkQualitiesPrefDelegate::RegisterPrefs(pref_registry.get());
    pref_service = pref_service_factory.Create(pref_registry.get());

    builder.SetHttpServerProperties(std::make_unique<net::HttpServerProperties>(
        std::make_unique<HttpServerPropertiesPrefDelegate>(pref_service.get()),
        net_log));

    network_qualities_pref_delegate_ =
        std::make_unique<NetworkQualitiesPrefDelegate>(
            pref_service.get(), network_service_->network_quality_estimator());
  }

  base::FilePath transport_security_persister_file_name;
  if (GetFullDataFilePath(params_->file_paths,
                          &network::mojom::NetworkContextFilePaths::
                              transport_security_persister_file_name,
                          transport_security_persister_file_name)) {
    builder.set_transport_security_persister_file_path(
        transport_security_persister_file_name);
  }
  builder.set_hsts_policy_bypass_list(params_->hsts_policy_bypass_list);

#if BUILDFLAG(ENABLE_REPORTING)
  bool reporting_enabled = base::FeatureList::IsEnabled(features::kReporting);
  if (reporting_enabled) {
    auto reporting_policy = net::ReportingPolicy::Create();
    if (params_->reporting_delivery_interval) {
      reporting_policy->delivery_interval =
          *params_->reporting_delivery_interval;
      reporting_policy->endpoint_backoff_policy.initial_delay_ms =
          params_->reporting_delivery_interval->InMilliseconds();
    }
    builder.set_reporting_policy(std::move(reporting_policy));
  } else {
    builder.set_reporting_policy(nullptr);
  }

  bool nel_enabled =
      base::FeatureList::IsEnabled(features::kNetworkErrorLogging);
  builder.set_network_error_logging_enabled(nel_enabled);

  base::FilePath reporting_and_nel_store_database_name;
  if (GetFullDataFilePath(params_->file_paths,
                          &network::mojom::NetworkContextFilePaths::
                              reporting_and_nel_store_database_name,
                          reporting_and_nel_store_database_name) &&
      (reporting_enabled || nel_enabled)) {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(),
             net::GetReportingAndNelStoreBackgroundSequencePriority(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    std::unique_ptr<net::SQLitePersistentReportingAndNelStore> sqlite_store(
        new net::SQLitePersistentReportingAndNelStore(
            reporting_and_nel_store_database_name, client_task_runner,
            background_task_runner));
    builder.set_persistent_reporting_and_nel_store(std::move(sqlite_store));
  } else {
    builder.set_persistent_reporting_and_nel_store(nullptr);
  }

#endif  // BUILDFLAG(ENABLE_REPORTING)

  net::HttpNetworkSessionParams session_params;
  bool is_quic_force_disabled = false;
  if (network_service_ && network_service_->quic_disabled())
    is_quic_force_disabled = true;

  auto quic_context = std::make_unique<net::QuicContext>();
  network_session_configurator::ParseCommandLineAndFieldTrials(
      *base::CommandLine::ForCurrentProcess(), is_quic_force_disabled,
      &session_params, quic_context->params());

  session_params.disable_idle_sockets_close_on_memory_pressure =
      params_->disable_idle_sockets_close_on_memory_pressure;

  if (network_service_) {
    session_params.key_auth_cache_server_entries_by_network_anonymization_key =
        network_service_->split_auth_cache_by_network_isolation_key();
  }

  session_params.key_auth_cache_server_entries_by_network_anonymization_key =
      base::FeatureList::IsEnabled(
          features::kSplitAuthCacheByNetworkIsolationKey);

  builder.set_http_network_session_params(session_params);
  builder.set_quic_context(std::move(quic_context));

  if (params_->shared_dictionary_enabled) {
    CHECK(GetSharedDictionaryManager());
    builder.SetCreateHttpTransactionFactoryCallback(base::BindOnce(
        [](base::WeakPtr<NetworkContext> context,
           net::HttpNetworkSession* session)
            -> std::unique_ptr<net::HttpTransactionFactory> {
          CHECK(context);
          return std::make_unique<SharedDictionaryNetworkTransactionFactory>(
              *context->GetSharedDictionaryManager(),
              std::make_unique<ThrottlingNetworkTransactionFactory>(session));
        },
        weak_factory_.GetWeakPtr()));
  } else {
    builder.SetCreateHttpTransactionFactoryCallback(
        base::BindOnce([](net::HttpNetworkSession* session)
                           -> std::unique_ptr<net::HttpTransactionFactory> {
          return std::make_unique<ThrottlingNetworkTransactionFactory>(session);
        }));
  }

  builder.set_host_mapping_rules(
      command_line->GetSwitchValueASCII(switches::kHostResolverRules));

  if (params_->socket_broker) {
    builder.set_client_socket_factory(
        std::make_unique<BrokeredClientSocketFactory>(
            std::move(params_->socket_broker)));
  }

  // If `require_network_anonymization_key_` is true, but the features that can
  // trigger another URLRequest are not set to respect NetworkAnonymizationKeys,
  // the URLRequests that they create might not have a NAK, so only set the
  // corresponding value in the URLRequestContext to true at the URLRequest
  // layer if all those features are set to respect NAK.
  if (require_network_anonymization_key_ &&
      net::NetworkAnonymizationKey::IsPartitioningEnabled() &&
      base::FeatureList::IsEnabled(
          domain_reliability::features::
              kPartitionDomainReliabilityByNetworkIsolationKey)) {
    builder.set_require_network_anonymization_key(true);
  }

#if BUILDFLAG(IS_ANDROID)
  builder.set_check_cleartext_permitted(params_->check_clear_text_permitted);
#endif  // BUILDFLAG(IS_ANDROID)

  if (params_->cookie_deprecation_label.has_value()) {
    builder.set_cookie_deprecation_label(*params_->cookie_deprecation_label);
  }

  if (on_url_request_context_builder_configured) {
    std::move(on_url_request_context_builder_configured).Run(&builder);
  }
  auto result =
      URLRequestContextOwner(std::move(pref_service), builder.Build());

  require_network_anonymization_key_ =
      params_->require_network_anonymization_key;

  // Subscribe the CertVerifier to configuration changes that are exposed via
  // the mojom::SSLConfig, but which are not part of the
  // net::SSLConfig[Service] interfaces.
  ssl_config_service_raw->SetCertVerifierForConfiguring(
      result.url_request_context->cert_verifier());

  // Attach some things to the URLRequestContextBuilder's
  // TransportSecurityState.  Since no requests have been made yet, safe to do
  // this even after the call to Build().

  if (params_->enable_certificate_reporting) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("domain_security_policy", R"(
        semantics {
          sender: "Domain Security Policy"
          description:
            "Websites can opt in to have Chrome send reports to them when "
            "Chrome observes connections to that website that do not meet "
            "stricter security policies, such as with HTTP Public Key Pinning. "
            "Websites can use this feature to discover misconfigurations that "
            "prevent them from complying with stricter security policies that "
            "they\'ve opted in to."
          trigger:
            "Chrome observes that a user is loading a resource from a website "
            "that has opted in for security policy reports, and the connection "
            "does not meet the required security policies."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and sometimes certificate revocation "
            "information included on the connection."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, this is a feature that websites can opt into and "
            "thus there is no Chrome-wide policy to disable it."
        })");
    certificate_report_sender_ = std::make_unique<net::ReportSender>(
        result.url_request_context.get(), traffic_annotation);
    result.url_request_context->transport_security_state()->SetReportSender(
        certificate_report_sender_.get());
  }

  if (network_service_->pins_list_updated()) {
    result.url_request_context->transport_security_state()->UpdatePinList(
        network_service_->pinsets(), network_service_->host_pins(),
        network_service_->pins_list_update_time());
  }

#if BUILDFLAG(IS_CT_SUPPORTED)
  if (params_->enforce_chrome_ct_policy) {
    require_ct_delegate_ =
        std::make_unique<certificate_transparency::ChromeRequireCTDelegate>();
    result.url_request_context->transport_security_state()
        ->SetRequireCTDelegate(require_ct_delegate_.get());
  }
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  if (params_->enable_domain_reliability) {
    domain_reliability_monitor_ =
        std::make_unique<domain_reliability::DomainReliabilityMonitor>(
            result.url_request_context.get(),
            params_->domain_reliability_upload_reporter,
            base::BindRepeating(&NetworkContext::CanUploadDomainReliability,
                                base::Unretained(this)));
    domain_reliability_monitor_->AddBakedInConfigs();
    domain_reliability_monitor_->SetDiscardUploads(
        params_->discard_domain_reliablity_uploads);
  }

  if (proxy_delegate_) {
    proxy_delegate_->SetProxyResolutionService(
        result.url_request_context->proxy_resolution_service());

    if (params_->ip_protection_config_getter) {
      proxy_delegate_->SetIpProtectionConfigCache(
          std::make_unique<IpProtectionConfigCacheImpl>(
              std::move(params_->ip_protection_config_getter)));
    }
  }

  return result;
}

scoped_refptr<SessionCleanupCookieStore>
NetworkContext::MakeSessionCleanupCookieStore() const {
  base::FilePath cookie_path;
  if (!GetFullDataFilePath(
          params_->file_paths,
          &network::mojom::NetworkContextFilePaths::cookie_database_name,
          cookie_path)) {
    DCHECK(!params_->restore_old_session_cookies);
    DCHECK(!params_->persist_session_cookies);
    return nullptr;
  }
  scoped_refptr<base::SequencedTaskRunner> client_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), net::GetCookieStoreBackgroundSequencePriority(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  net::CookieCryptoDelegate* crypto_delegate = nullptr;
  if (params_->enable_encrypted_cookies) {
    crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  }

#if BUILDFLAG(IS_WIN)
  const bool enable_exclusive_access = params_->enable_locking_cookie_database;
#else
  const bool enable_exclusive_access = false;
#endif  // BUILDFLAG(IS_WIN)
  scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
      new net::SQLitePersistentCookieStore(
          cookie_path, client_task_runner, background_task_runner,
          params_->restore_old_session_cookies, crypto_delegate,
          enable_exclusive_access));

  return base::MakeRefCounted<SessionCleanupCookieStore>(sqlite_store);
}

void NetworkContext::OnHttpCacheCleared(ClearHttpCacheCallback callback,
                                        HttpCacheDataRemover* remover) {
  bool removed = false;
  for (auto iter = http_cache_data_removers_.begin();
       iter != http_cache_data_removers_.end(); ++iter) {
    if (iter->get() == remover) {
      removed = true;
      http_cache_data_removers_.erase(iter);
      break;
    }
  }
  DCHECK(removed);
  std::move(callback).Run();
}

void NetworkContext::OnHostResolverShutdown(HostResolver* resolver) {
  auto found_resolver = host_resolvers_.find(resolver);
  DCHECK(found_resolver != host_resolvers_.end());
  host_resolvers_.erase(found_resolver);
}

void NetworkContext::OnHttpCacheSizeComputed(
    ComputeHttpCacheSizeCallback callback,
    HttpCacheDataCounter* counter,
    bool is_upper_limit,
    int64_t result_or_error) {
  EraseIf(http_cache_data_counters_, base::MatchesUniquePtr(counter));
  std::move(callback).Run(is_upper_limit, result_or_error);
}

void NetworkContext::OnConnectionError() {
  // If owned by the network service, this call will delete |this|.
  if (on_connection_close_callback_)
    std::move(on_connection_close_callback_).Run(this);
}

GURL NetworkContext::GetHSTSRedirect(const GURL& original_url) {
  // TODO(lilyhoughton) This needs to be gotten rid of once explicit
  // construction with a URLRequestContext is no longer supported.
  if (!url_request_context_->transport_security_state() ||
      !original_url.SchemeIs("http") ||
      !url_request_context_->transport_security_state()->ShouldUpgradeToSSL(
          original_url.host())) {
    return original_url;
  }

  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  return original_url.ReplaceComponents(replacements);
}

#if BUILDFLAG(IS_P2P_ENABLED)
void NetworkContext::DestroySocketManager(P2PSocketManager* socket_manager) {
  auto iter = socket_managers_.find(socket_manager);
  DCHECK(iter != socket_managers_.end());
  socket_managers_.erase(iter);
}
#endif  // BUILDFLAG(IS_P2P_ENABLED)

void NetworkContext::CanUploadDomainReliability(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  client_->OnCanSendDomainReliabilityUpload(
      origin,
      base::BindOnce([](base::OnceCallback<void(bool)> callback,
                        bool allowed) { std::move(callback).Run(allowed); },
                     std::move(callback)));
}

void NetworkContext::OnVerifyCertForSignedExchangeComplete(
    uint64_t cert_verify_id,
    int result) {
  auto iter = cert_verifier_requests_.find(cert_verify_id);
  DCHECK(iter != cert_verifier_requests_.end());

  auto pending_cert_verify = std::move(iter->second);
  cert_verifier_requests_.erase(iter);

  bool pkp_bypassed = false;
  std::string pinning_failure_log;
  if (result == net::OK) {
#if BUILDFLAG(IS_CT_SUPPORTED)
    int ct_result = CheckCTComplianceForSignedExchange(
        *pending_cert_verify->result, *pending_cert_verify->certificate,
        net::HostPortPair::FromURL(pending_cert_verify->url));
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
    net::TransportSecurityState::PKPStatus pin_validity =
        url_request_context_->transport_security_state()->CheckPublicKeyPins(
            net::HostPortPair::FromURL(pending_cert_verify->url),
            pending_cert_verify->result->is_issued_by_known_root,
            pending_cert_verify->result->public_key_hashes,
            pending_cert_verify->certificate.get(),
            pending_cert_verify->result->verified_cert.get(),
            net::TransportSecurityState::ENABLE_PIN_REPORTS,
            pending_cert_verify->network_anonymization_key,
            &pinning_failure_log);
    switch (pin_validity) {
      case net::TransportSecurityState::PKPStatus::VIOLATED:
        pending_cert_verify->result->cert_status |=
            net::CERT_STATUS_PINNED_KEY_MISSING;
        result = net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        break;
      case net::TransportSecurityState::PKPStatus::BYPASSED:
        pkp_bypassed = true;
        [[fallthrough]];
      case net::TransportSecurityState::PKPStatus::OK:
        // Do nothing.
        break;
    }
#if BUILDFLAG(IS_CT_SUPPORTED)
    if (result != net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN &&
        ct_result != net::OK)
      result = ct_result;
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  }

  std::move(pending_cert_verify->callback)
      .Run(result, *pending_cert_verify->result.get(), pkp_bypassed,
           pinning_failure_log);
}

#if BUILDFLAG(IS_CHROMEOS)
void NetworkContext::TrustAnchorUsed() {
  client_->OnTrustAnchorUsed();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
void NetworkContext::EnsureMounted(network::TransferableDirectory* directory) {
  if (directory->NeedsMount()) {
    dismount_closures_.push_back(directory->Mount());
  }
}
#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

void NetworkContext::InitializeCorsParams() {
  for (const auto& pattern : params_->cors_origin_access_list) {
    cors_origin_access_list_.SetAllowListForOrigin(pattern->source_origin,
                                                   pattern->allow_patterns);
    cors_origin_access_list_.SetBlockListForOrigin(pattern->source_origin,
                                                   pattern->block_patterns);
  }
  for (const auto& key : params_->cors_exempt_header_list)
    cors_exempt_header_list_.insert(key);

  acam_preflight_spec_conformant_ =
      base::FeatureList::IsEnabled(
          network::features::
              kAccessControlAllowMethodsInCORSPreflightSpecConformant) &&
      params_->acam_preflight_spec_conformant;
}

void NetworkContext::FinishConstructingTrustTokenStore(
    std::unique_ptr<SQLiteTrustTokenPersister> persister) {
  trust_token_store_->OnStoreReady(std::make_unique<TrustTokenStore>(
      std::move(persister),
      std::make_unique<ExpiryInspectingRecordExpiryDelegate>(
          network_service()->trust_token_key_commitments())));
}

bool NetworkContext::IsAllowedToUseAllHttpAuthSchemes(
    const url::SchemeHostPort& scheme_host_port) {
  DCHECK(url_matcher_);
  return !url_matcher_->MatchURL(scheme_host_port.GetURL()).empty();
}

void NetworkContext::CreateTrustedUrlLoaderFactoryForNetworkService(
    mojo::PendingReceiver<mojom::URLLoaderFactory>
        url_loader_factory_pending_receiver) {
  auto url_loader_factory_params = mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->is_trusted = true;
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  CreateURLLoaderFactory(std::move(url_loader_factory_pending_receiver),
                         std::move(url_loader_factory_params));
}

void NetworkContext::SetSharedDictionaryCacheMaxSize(uint64_t cache_max_size) {
  if (!shared_dictionary_manager_) {
    return;
  }
  shared_dictionary_manager_->SetCacheMaxSize(cache_max_size);
}

void NetworkContext::ClearSharedDictionaryCache(
    base::Time start_time,
    base::Time end_time,
    mojom::ClearDataFilterPtr filter,
    ClearSharedDictionaryCacheCallback callback) {
  if (!shared_dictionary_manager_) {
    std::move(callback).Run();
    return;
  }
  shared_dictionary_manager_->ClearData(
      start_time, end_time,
      filter
          ? base::BindRepeating(&DoesUrlMatchFilter, filter->type,
                                std::set<url::Origin>(filter->origins.begin(),
                                                      filter->origins.end()),
                                std::set<std::string>(filter->domains.begin(),
                                                      filter->domains.end()))
          : base::RepeatingCallback<bool(const GURL&)>(),
      std::move(callback));
}

void NetworkContext::ClearSharedDictionaryCacheForIsolationKey(
    const net::SharedDictionaryIsolationKey& isolation_key,
    ClearSharedDictionaryCacheForIsolationKeyCallback callback) {
  if (!shared_dictionary_manager_) {
    std::move(callback).Run();
    return;
  }
  shared_dictionary_manager_->ClearDataForIsolationKey(isolation_key,
                                                       std::move(callback));
}

void NetworkContext::GetSharedDictionaryUsageInfo(
    GetSharedDictionaryUsageInfoCallback callback) {
  if (!shared_dictionary_manager_) {
    std::move(callback).Run({});
    return;
  }
  shared_dictionary_manager_->GetUsageInfo(std::move(callback));
}

void NetworkContext::GetSharedDictionaryInfo(
    const net::SharedDictionaryIsolationKey& isolation_key,
    GetSharedDictionaryInfoCallback callback) {
  if (!shared_dictionary_manager_) {
    std::move(callback).Run({});
    return;
  }
  shared_dictionary_manager_->GetSharedDictionaryInfo(isolation_key,
                                                      std::move(callback));
}

void NetworkContext::GetSharedDictionaryOriginsBetween(
    base::Time start_time,
    base::Time end_time,
    GetSharedDictionaryOriginsBetweenCallback callback) {
  if (!shared_dictionary_manager_) {
    std::move(callback).Run({});
    return;
  }
  shared_dictionary_manager_->GetOriginsBetween(start_time, end_time,
                                                std::move(callback));
}

void NetworkContext::ResourceSchedulerClientVisibilityChanged(
    const base::UnguessableToken& client_token,
    bool visible) {
  resource_scheduler_->OnClientVisibilityChanged(client_token, visible);
}

void NetworkContext::FlushCachedClientCertIfNeeded(
    const net::HostPortPair& host,
    const scoped_refptr<net::X509Certificate>& certificate) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);
  if (http_session->ssl_client_context()) {
    http_session->ssl_client_context()->ClearClientCertificateIfNeeded(
        host, certificate);
  }
}

}  // namespace network
