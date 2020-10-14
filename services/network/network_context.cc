// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/domain_reliability/monitor.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_cache.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cookie_manager.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/host_resolver.h"
#include "services/network/http_auth_cache_copier.h"
#include "services/network/http_server_properties_pref_delegate.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_service.h"
#include "services/network/network_service_network_delegate.h"
#include "services/network/network_service_proxy_delegate.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/p2p/socket_manager.h"
#include "services/network/proxy_config_service_mojo.h"
#include "services/network/proxy_lookup_request.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/public/cpp/cert_verifier/cert_verifier_creation.h"
#include "services/network/public/cpp/cert_verifier/mojo_cert_verifier.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/quic_transport.h"
#include "services/network/resolve_host_request.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "services/network/ssl_config_service_mojo.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_transaction_factory.h"
#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"
#include "services/network/trust_tokens/has_trust_tokens_answerer.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/sqlite_trust_token_persister.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_builder_mojo.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "components/certificate_transparency/chrome_require_ct_delegate.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "services/network/expect_ct_reporter.h"
#include "services/network/sct_auditing_cache.h"
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
#include "net/ftp/ftp_auth_cache.h"
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if defined(OS_CHROMEOS)
#include "services/network/cert_verifier_with_trust_anchors.h"
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_IOS)
#include "services/network/websocket_factory.h"
#endif  // !defined(OS_IOS)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/base/http_user_agent_settings.h"
#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(ENABLE_MDNS)
#include "services/network/mdns_responder.h"
#endif  // BUILDFLAG(ENABLE_MDNS)

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace network {

namespace {

#if BUILDFLAG(IS_CT_SUPPORTED)
// A Base-64 encoded DER certificate for use in test Expect-CT reports. The
// contents of the certificate don't matter.
const char kTestReportCert[] =
    "MIIDvzCCAqegAwIBAgIBAzANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET"
    "MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G"
    "A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3"
    "MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh"
    "bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg"
    "Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC"
    "AQoCggEBALS/0pcz5RNbd2W9cxp1KJtHWea3MOhGM21YW9ofCv/k5C3yHfiJ6GQu"
    "9sPN16OO1/fN59gOEMPnVtL85ebTTuL/gk0YY4ewo97a7wo3e6y1t0PO8gc53xTp"
    "w6RBPn5oRzSbe2HEGOYTzrO0puC6A+7k6+eq9G2+l1uqBpdQAdB4uNaSsOTiuUOI"
    "ta4UZH1ScNQFHAkl1eJPyaiC20Exw75EbwvU/b/B7tlivzuPtQDI0d9dShOtceRL"
    "X9HZckyD2JNAv2zNL2YOBNa5QygkySX9WXD+PfKpCk7Cm8TenldeXRYl5ni2REkp"
    "nfa/dPuF1g3xZVjyK9aPEEnIAC2I4i0CAwEAAaOBgDB+MAwGA1UdEwEB/wQCMAAw"
    "HQYDVR0OBBYEFODc4C8HiHQ6n9Mwo3GK+dal5aZTMB8GA1UdIwQYMBaAFJsmC4qY"
    "qbsduR8c4xpAM+2OF4irMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAP"
    "BgNVHREECDAGhwR/AAABMA0GCSqGSIb3DQEBCwUAA4IBAQB6FEQuUDRcC5jkX3aZ"
    "uuTeZEqMVL7JXgvgFqzXsPb8zIdmxr/tEDfwXx2qDf2Dpxts7Fq4vqUwimK4qV3K"
    "7heLnWV2+FBvV1eeSfZ7AQj+SURkdlyo42r41+t13QUf+Z0ftR9266LSWLKrukeI"
    "Mxk73hOkm/u8enhTd00dy/FN9dOFBFHseVMspWNxIkdRILgOmiyfQNRgxNYdOf0e"
    "EfELR8Hn6WjZ8wAbvO4p7RTrzu1c/RZ0M+NLkID56Brbl70GC2h5681LPwAOaZ7/"
    "mWQ5kekSyJjmLfF12b+h9RVAt5MrXZgk2vNujssgGf4nbWh4KZyQ6qrs778ZdDLm"
    "yfUn";
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

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

// Predicate function to determine if the given |url| matches the |filter_type|,
// |filter_domains| and |filter_origins| from a |mojom::ClearDataFilter|.
bool MatchesUrlFilter(mojom::ClearDataFilter_Type filter_type,
                      std::set<std::string> filter_domains,
                      std::set<url::Origin> filter_origins,
                      const GURL& url) {
  std::string url_registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain =
      (filter_domains.find(url_registrable_domain != ""
                               ? url_registrable_domain
                               : url.host()) != filter_domains.end());

  bool found_origin =
      (filter_origins.find(url::Origin::Create(url)) != filter_origins.end());

  return (filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES) ==
         (found_domain || found_origin);
}

// Builds a generic GURL-matching predicate function based on |filter|. If
// |filter| is null, creates an always-true predicate.
base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter(
    mojom::ClearDataFilterPtr filter) {
  if (!filter) {
    return base::BindRepeating([](const GURL&) { return true; });
  }

  std::set<std::string> filter_domains;
  filter_domains.insert(filter->domains.begin(), filter->domains.end());

  std::set<url::Origin> filter_origins;
  filter_origins.insert(filter->origins.begin(), filter->origins.end());

  return base::BindRepeating(&MatchesUrlFilter, filter->type,
                             std::move(filter_domains),
                             std::move(filter_origins));
}

#if defined(OS_ANDROID)
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
#endif

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
  bool IsSCTAuditingEnabled() override;

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

bool SCTAuditingDelegate::IsSCTAuditingEnabled() {
  if (!context_)
    return false;
  return context_->is_sct_auditing_enabled();
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

}  // namespace

constexpr uint32_t NetworkContext::kMaxOutstandingRequestsPerProcess;

NetworkContext::PendingCertVerify::PendingCertVerify() = default;
NetworkContext::PendingCertVerify::~PendingCertVerify() = default;

// net::NetworkDelegate that wraps
NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    mojom::NetworkContextParamsPtr params,
    OnConnectionCloseCallback on_connection_close_callback)
    : network_service_(network_service),
      url_request_context_(nullptr),
      params_(std::move(params)),
      on_connection_close_callback_(std::move(on_connection_close_callback)),
#if defined(OS_ANDROID)
      app_status_listener_(
          std::make_unique<NetworkContextApplicationStatusListener>()),
#endif
      receiver_(this, std::move(receiver)),
      cors_preflight_controller_(network_service) {
  mojo::PendingRemote<mojom::URLLoaderFactory>
      url_loader_factory_for_cert_net_fetcher;
  mojo::PendingReceiver<mojom::URLLoaderFactory>
      url_loader_factory_for_cert_net_fetcher_receiver =
          url_loader_factory_for_cert_net_fetcher
              .InitWithNewPipeAndPassReceiver();

  url_request_context_owner_ =
      MakeURLRequestContext(std::move(url_loader_factory_for_cert_net_fetcher));
  url_request_context_ = url_request_context_owner_.url_request_context.get();

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

  origin_policy_manager_ = std::make_unique<OriginPolicyManager>(this);

  if (params_->http_auth_static_network_context_params) {
    http_auth_merged_preferences_.SetAllowDefaultCredentials(
        params_->http_auth_static_network_context_params
            ->allow_default_credentials);
  }

  InitializeCorsParams();

  SetSplitAuthCacheByNetworkIsolationKey(
      params_->split_auth_cache_by_network_isolation_key);

#if BUILDFLAG(IS_CT_SUPPORTED)
  if (params_->ct_policy)
    SetCTPolicy(std::move(params_->ct_policy));
  SetSCTAuditingEnabled(params_->enable_sct_auditing);
#endif

#if defined(OS_ANDROID)
  if (params_->cookie_manager)
    GetCookieManager(std::move(params_->cookie_manager));
#endif

  CreateURLLoaderFactoryForCertNetFetcher(
      std::move(url_loader_factory_for_cert_net_fetcher_receiver));
}

NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojo::PendingReceiver<mojom::NetworkContext> receiver,
    net::URLRequestContext* url_request_context,
    const std::vector<std::string>& cors_exempt_header_list)
    : network_service_(network_service),
      url_request_context_(url_request_context),
#if defined(OS_ANDROID)
      app_status_listener_(
          std::make_unique<NetworkContextApplicationStatusListener>()),
#endif
      receiver_(this, std::move(receiver)),
      cookie_manager_(
          std::make_unique<CookieManager>(url_request_context->cookie_store(),
                                          nullptr,
                                          nullptr)),
      socket_factory_(
          std::make_unique<SocketFactory>(url_request_context_->net_log(),
                                          url_request_context)),
      cors_preflight_controller_(network_service) {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->RegisterNetworkContext(this);
  resource_scheduler_ = std::make_unique<ResourceScheduler>();

  for (const auto& key : cors_exempt_header_list)
    cors_exempt_header_list_.insert(key);

  origin_policy_manager_ = std::make_unique<OriginPolicyManager>(this);
}

NetworkContext::~NetworkContext() {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->DeregisterNetworkContext(this);

  if (cert_net_fetcher_)
    cert_net_fetcher_->Shutdown();

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
    if (expect_ct_reporter_) {
      url_request_context_->transport_security_state()->SetExpectCTReporter(
          nullptr);
      expect_ct_reporter_.reset();
    }

    if (require_ct_delegate_) {
      url_request_context_->transport_security_state()->SetRequireCTDelegate(
          nullptr);
    }
#endif  // BUILDFLAG(IS_CT_SUPPORTED)
  }
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
      std::move(receiver), &cors_origin_access_list_));
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
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client;
  resource_scheduler_client = base::MakeRefCounted<ResourceSchedulerClient>(
      params->process_id, ++current_resource_scheduler_client_id_,
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

void NetworkContext::GetCookieManager(
    mojo::PendingReceiver<mojom::CookieManager> receiver) {
  cookie_manager_->AddReceiver(std::move(receiver));
}

void NetworkContext::GetRestrictedCookieManager(
    mojo::PendingReceiver<mojom::RestrictedCookieManager> receiver,
    mojom::RestrictedCookieManagerRole role,
    const url::Origin& origin,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer) {
  mojom::NetworkServiceClient* network_service_client = nullptr;
  if (network_service())
    network_service_client = network_service()->client();

  restricted_cookie_manager_receivers_.Add(
      std::make_unique<RestrictedCookieManager>(
          role, url_request_context_->cookie_store(),
          &cookie_manager_->cookie_settings(), origin, site_for_cookies,
          top_frame_origin, std::move(cookie_observer)),
      std::move(receiver));
}

void NetworkContext::GetHasTrustTokensAnswerer(
    mojo::PendingReceiver<mojom::HasTrustTokensAnswerer> receiver,
    const url::Origin& top_frame_origin) {
  // Only called when Trust Tokens is enabled, i.e. trust_token_store_ is
  // non-null.
  DCHECK(trust_token_store_);

  base::Optional<SuitableTrustTokenOrigin> suitable_top_frame_origin =
      SuitableTrustTokenOrigin::Create(top_frame_origin);

  // It's safe to dereference |suitable_top_frame_origin| here as, during the
  // process of vending the HasTrustTokensAnswerer, the browser ensures that
  // the requesting context's top frame origin is suitable for Trust Tokens.
  auto answerer = std::make_unique<HasTrustTokensAnswerer>(
      std::move(*suitable_top_frame_origin), trust_token_store_.get());

  has_trust_tokens_answerers_.Add(std::move(answerer), std::move(receiver));
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
  const int32_t process_id = url_loader_factory->process_id();

  auto it = url_loader_factories_.find(url_loader_factory);
  DCHECK(it != url_loader_factories_.end());
  url_loader_factories_.erase(it);

  // Reset bytes transferred for the process if |url_loader_factory| is the
  // last factory associated with the process.
  if (network_service() &&
      std::none_of(url_loader_factories_.cbegin(), url_loader_factories_.cend(),
                   [process_id](const auto& factory) {
                     return factory->process_id() == process_id;
                   })) {
    network_service()
        ->network_usage_accumulator()
        ->ClearBytesTransferredForProcess(process_id);
  }
}

void NetworkContext::Remove(QuicTransport* transport) {
  auto it = quic_transports_.find(transport);
  if (it != quic_transports_.end()) {
    quic_transports_.erase(it);
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
  return params_ && params_->skip_reporting_send_permission_check;
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
        ignore_result(store->ClearDataForFilter(std::move(filter)));
        std::move(done).Run();
      },
      std::move(filter), std::move(done)));
}

void NetworkContext::ClearNetworkingHistoryBetween(
    base::Time start_time,
    base::Time end_time,
    base::OnceClosure completion_callback) {
  auto barrier = base::BarrierClosure(2, std::move(completion_callback));

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

#if BUILDFLAG(ENABLE_REPORTING)
void NetworkContext::ClearReportingCacheReports(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheReportsCallback callback) {
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
          BuildUrlFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS);
    }
  }

  std::move(callback).Run();
}

void NetworkContext::ClearReportingCacheClients(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheClientsCallback callback) {
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
          BuildUrlFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
    }
  }

  std::move(callback).Run();
}

void NetworkContext::ClearNetworkErrorLogging(
    mojom::ClearDataFilterPtr filter,
    ClearNetworkErrorLoggingCallback callback) {
  net::NetworkErrorLoggingService* logging_service =
      url_request_context_->network_error_logging_service();
  if (logging_service) {
    if (filter) {
      logging_service->RemoveBrowsingData(BuildUrlFilter(std::move(filter)));
    } else {
      logging_service->RemoveAllBrowsingData();
    }
  }

  std::move(callback).Run();
}

void NetworkContext::QueueReport(const std::string& type,
                                 const std::string& group,
                                 const GURL& url,
                                 const base::Optional<std::string>& user_agent,
                                 base::Value body) {
  DCHECK(body.is_dict());
  if (!body.is_dict())
    return;

  // Get the ReportingService.
  net::URLRequestContext* request_context = url_request_context();
  net::ReportingService* reporting_service =
      request_context->reporting_service();
  // TODO(paulmeyer): Remove this once the network service ships everywhere.
  if (!reporting_service) {
    net::ReportingReport::RecordReportDiscardedForNoReportingService();
    return;
  }

  std::string reported_user_agent = user_agent.value_or("");
  if (reported_user_agent.empty() &&
      request_context->http_user_agent_settings() != nullptr) {
    reported_user_agent =
        request_context->http_user_agent_settings()->GetUserAgent();
  }

  // Send the crash report to the Reporting API.
  reporting_service->QueueReport(url, reported_user_agent, group, type,
                                 base::Value::ToUniquePtrValue(std::move(body)),
                                 0 /* depth */);
}

void NetworkContext::QueueSignedExchangeReport(
    mojom::SignedExchangeReportPtr report) {
  net::URLRequestContext* request_context = url_request_context();
  net::NetworkErrorLoggingService* logging_service =
      request_context->network_error_logging_service();
  if (!logging_service)
    return;
  std::string user_agent;
  if (request_context->http_user_agent_settings() != nullptr) {
    user_agent = request_context->http_user_agent_settings()->GetUserAgent();
  }
  net::NetworkErrorLoggingService::SignedExchangeReportDetails details;
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
}

#else   // BUILDFLAG(ENABLE_REPORTING)
void NetworkContext::ClearReportingCacheReports(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheReportsCallback callback) {
  NOTREACHED();
}

void NetworkContext::ClearReportingCacheClients(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheClientsCallback callback) {
  NOTREACHED();
}

void NetworkContext::ClearNetworkErrorLogging(
    mojom::ClearDataFilterPtr filter,
    ClearNetworkErrorLoggingCallback callback) {
  NOTREACHED();
}

void NetworkContext::QueueReport(const std::string& type,
                                 const std::string& group,
                                 const GURL& url,
                                 const base::Optional<std::string>& user_agent,
                                 base::Value body) {
  NOTREACHED();
}

void NetworkContext::QueueSignedExchangeReport(
    mojom::SignedExchangeReportPtr report) {
  NOTREACHED();
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
        dr_mode, BuildUrlFilter(std::move(filter)));
  }
  std::move(callback).Run();
}

void NetworkContext::GetDomainReliabilityJSON(
    GetDomainReliabilityJSONCallback callback) {
  if (!domain_reliability_monitor_) {
    base::DictionaryValue data;
    data.SetString("error", "no_service");
    std::move(callback).Run(std::move(data));
    return;
  }

  std::move(callback).Run(
      std::move(*domain_reliability_monitor_->GetWebUIData()));
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
    network_conditions.reset(new NetworkConditions(
        conditions->offline, conditions->latency.InMillisecondsF(),
        conditions->download_throughput, conditions->upload_throughput));
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

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CT_SUPPORTED)
void NetworkContext::SetCTPolicy(mojom::CTPolicyPtr ct_policy) {
  if (!require_ct_delegate_)
    return;

  require_ct_delegate_->UpdateCTPolicies(
      ct_policy->required_hosts, ct_policy->excluded_hosts,
      ct_policy->excluded_spkis, ct_policy->excluded_legacy_spkis);
}

void NetworkContext::AddExpectCT(
    const std::string& domain,
    base::Time expiry,
    bool enforce,
    const GURL& report_uri,
    const net::NetworkIsolationKey& network_isolation_key,
    AddExpectCTCallback callback) {
  net::TransportSecurityState* transport_security_state =
      url_request_context()->transport_security_state();
  if (!transport_security_state) {
    std::move(callback).Run(false);
    return;
  }

  transport_security_state->AddExpectCT(domain, expiry, enforce, report_uri,
                                        network_isolation_key);
  std::move(callback).Run(true);
}

void NetworkContext::SetExpectCTTestReport(
    const GURL& report_uri,
    SetExpectCTTestReportCallback callback) {
  std::string decoded_dummy_cert;
  DCHECK(base::Base64Decode(kTestReportCert, &decoded_dummy_cert));
  scoped_refptr<net::X509Certificate> dummy_cert =
      net::X509Certificate::CreateFromBytes(decoded_dummy_cert.data(),
                                            decoded_dummy_cert.size());

  LazyCreateExpectCTReporter(url_request_context());

  // We need to save |callback| into a queue because this implementation is
  // relying on the success/failed observer methods of network::ExpectCTReporter
  // which can be called at any time, and for other reasons. It's unlikely
  // but it is possible that |callback| could be called for some other event
  // other than the one initiated below when calling OnExpectCTFailed.
  outstanding_set_expect_ct_callbacks_.push(std::move(callback));

  // Send a test report with dummy data.
  net::SignedCertificateTimestampAndStatusList dummy_sct_list;
  expect_ct_reporter_->OnExpectCTFailed(
      net::HostPortPair("expect-ct-report.test", 443), report_uri,
      base::Time::Now(), dummy_cert.get(), dummy_cert.get(), dummy_sct_list,
      // No need for a shared NetworkIsolationKey here, as this is test-only
      // code and none
      // of the tests that call it care about the NetworkIsolationKey.
      net::NetworkIsolationKey::CreateTransient());
}

void NetworkContext::LazyCreateExpectCTReporter(
    net::URLRequestContext* url_request_context) {
  if (expect_ct_reporter_)
    return;

  // This instance owns owns and outlives expect_ct_reporter_, so safe to
  // pass |this|.
  expect_ct_reporter_ = std::make_unique<network::ExpectCTReporter>(
      url_request_context,
      base::BindRepeating(&NetworkContext::OnSetExpectCTTestReportSuccess,
                          base::Unretained(this)),
      base::BindRepeating(&NetworkContext::OnSetExpectCTTestReportFailure,
                          base::Unretained(this)));
}

void NetworkContext::OnSetExpectCTTestReportSuccess() {
  if (outstanding_set_expect_ct_callbacks_.empty())
    return;
  std::move(outstanding_set_expect_ct_callbacks_.front()).Run(true);
  outstanding_set_expect_ct_callbacks_.pop();
}

void NetworkContext::OnSetExpectCTTestReportFailure() {
  if (outstanding_set_expect_ct_callbacks_.empty())
    return;
  std::move(outstanding_set_expect_ct_callbacks_.front()).Run(false);
  outstanding_set_expect_ct_callbacks_.pop();
}

void NetworkContext::GetExpectCTState(
    const std::string& domain,
    const net::NetworkIsolationKey& network_isolation_key,
    GetExpectCTStateCallback callback) {
  base::DictionaryValue result;
  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        url_request_context()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::ExpectCTState dynamic_expect_ct_state;
      bool found = transport_security_state->GetDynamicExpectCTState(
          domain, network_isolation_key, &dynamic_expect_ct_state);

      // TODO(estark): query static Expect-CT state as well.
      if (found) {
        result.SetString("dynamic_expect_ct_domain", domain);
        result.SetDouble("dynamic_expect_ct_observed",
                         dynamic_expect_ct_state.last_observed.ToDoubleT());
        result.SetDouble("dynamic_expect_ct_expiry",
                         dynamic_expect_ct_state.expiry.ToDoubleT());
        result.SetBoolean("dynamic_expect_ct_enforce",
                          dynamic_expect_ct_state.enforce);
        result.SetString("dynamic_expect_ct_report_uri",
                         dynamic_expect_ct_state.report_uri.spec());
      }

      result.SetBoolean("result", found);
    } else {
      result.SetString("error", "no Expect-CT state active");
    }
  } else {
    result.SetString("error", "non-ASCII domain name");
  }

  std::move(callback).Run(std::move(result));
}

void NetworkContext::MaybeEnqueueSCTReport(
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  network_service()->sct_auditing_cache()->MaybeEnqueueReport(
      this, host_port_pair, validated_certificate_chain,
      signed_certificate_timestamps);
}

void NetworkContext::SetSCTAuditingEnabled(bool enabled) {
  is_sct_auditing_enabled_ = enabled;
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

void NetworkContext::CreateUDPSocket(
    mojo::PendingReceiver<mojom::UDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener) {
  socket_factory_->CreateUDPSocket(std::move(receiver), std::move(listener));
}

void NetworkContext::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    uint32_t backlog,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    CreateTCPServerSocketCallback callback) {
  socket_factory_->CreateTCPServerSocket(
      local_addr, backlog,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(receiver), std::move(callback));
}

void NetworkContext::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
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
    const net::NetworkIsolationKey& network_isolation_key,
    mojo::PendingRemote<mojom::ProxyLookupClient> proxy_lookup_client) {
  DCHECK(proxy_lookup_client);
  std::unique_ptr<ProxyLookupRequest> proxy_lookup_request(
      std::make_unique<ProxyLookupRequest>(std::move(proxy_lookup_client), this,
                                           network_isolation_key));
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
    int32_t render_frame_id,
    const url::Origin& origin,
    uint32_t options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::AuthenticationHandler> auth_handler,
    mojo::PendingRemote<mojom::TrustedHeaderClient> header_client) {
#if !defined(OS_IOS)
  if (!websocket_factory_)
    websocket_factory_ = std::make_unique<WebSocketFactory>(this);

  DCHECK_GE(process_id, 0);
  if (process_id == mojom::kBrowserProcessId) {
    DCHECK_EQ(render_frame_id, 0);
  }

  websocket_factory_->CreateWebSocket(
      url, requested_protocols, site_for_cookies, isolation_info,
      std::move(additional_headers), process_id, render_frame_id, origin,
      options,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(handshake_client), std::move(auth_handler),
      std::move(header_client));
#endif  // !defined(OS_IOS)
}

void NetworkContext::CreateQuicTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkIsolationKey& key,
    std::vector<mojom::QuicTransportCertificateFingerprintPtr> fingerprints,
    mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
        pending_handshake_client) {
  quic_transports_.insert(
      std::make_unique<QuicTransport>(url, origin, key, fingerprints, this,
                                      std::move(pending_handshake_client)));
}

void NetworkContext::CreateNetLogExporter(
    mojo::PendingReceiver<mojom::NetLogExporter> receiver) {
  net_log_exporter_receivers_.Add(std::make_unique<NetLogExporter>(this),
                                  std::move(receiver));
}

void NetworkContext::ResolveHost(
    const net::HostPortPair& host,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<mojom::ResolveHostClient> response_client) {
  if (!internal_host_resolver_) {
    internal_host_resolver_ = std::make_unique<HostResolver>(
        url_request_context_->host_resolver(), url_request_context_->net_log());
  }

  internal_host_resolver_->ResolveHost(host, network_isolation_key,
                                       std::move(optional_parameters),
                                       std::move(response_client));
}

void NetworkContext::CreateHostResolver(
    const base::Optional<net::DnsConfigOverrides>& config_overrides,
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
    const std::string& ocsp_result,
    const std::string& sct_list,
    VerifyCertForSignedExchangeCallback callback) {
  int cert_verify_id = ++next_cert_verify_id_;
  auto pending_cert_verify = std::make_unique<PendingCertVerify>();
  pending_cert_verify->callback = std::move(callback);
  pending_cert_verify->result = std::make_unique<net::CertVerifyResult>();
  pending_cert_verify->certificate = certificate;
  pending_cert_verify->url = url;
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
      base::BindOnce(&NetworkContext::OnCertVerifyForSignedExchangeComplete,
                     base::Unretained(this), cert_verify_id),
      &pending_cert_verify->request,
      net::NetLogWithSource::Make(url_request_context_->net_log(),
                                  net::NetLogSourceType::CERT_VERIFIER_JOB));
  cert_verifier_requests_[cert_verify_id] = std::move(pending_cert_verify);

  if (result != net::ERR_IO_PENDING)
    OnCertVerifyForSignedExchangeComplete(cert_verify_id, result);
}

void NetworkContext::ParseHeaders(
    const GURL& url,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    ParseHeadersCallback callback) {
  std::move(callback).Run(PopulateParsedHeaders(headers, url));
}

void NetworkContext::NotifyExternalCacheHit(
    const GURL& url,
    const std::string& http_method,
    const net::NetworkIsolationKey& key) {
  net::HttpCache* cache =
      url_request_context_->http_transaction_factory()->GetCache();
  if (cache) {
    cache->OnExternalCacheHit(url, http_method, key);
  }
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
  base::DictionaryValue result;

  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        url_request_context()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::STSState static_sts_state;
      net::TransportSecurityState::PKPState static_pkp_state;
      bool found_static = transport_security_state->GetStaticDomainState(
          domain, &static_sts_state, &static_pkp_state);
      if (found_static) {
        result.SetInteger("static_upgrade_mode",
                          static_cast<int>(static_sts_state.upgrade_mode));
        result.SetBoolean("static_sts_include_subdomains",
                          static_sts_state.include_subdomains);
        result.SetDouble("static_sts_observed",
                         static_sts_state.last_observed.ToDoubleT());
        result.SetDouble("static_sts_expiry",
                         static_sts_state.expiry.ToDoubleT());
        result.SetBoolean("static_pkp_include_subdomains",
                          static_pkp_state.include_subdomains);
        result.SetDouble("static_pkp_observed",
                         static_pkp_state.last_observed.ToDoubleT());
        result.SetDouble("static_pkp_expiry",
                         static_pkp_state.expiry.ToDoubleT());
        result.SetString("static_spki_hashes",
                         HashesToBase64String(static_pkp_state.spki_hashes));
        result.SetString("static_sts_domain", static_sts_state.domain);
        result.SetString("static_pkp_domain", static_pkp_state.domain);
      }

      net::TransportSecurityState::STSState dynamic_sts_state;
      net::TransportSecurityState::PKPState dynamic_pkp_state;
      bool found_sts_dynamic = transport_security_state->GetDynamicSTSState(
          domain, &dynamic_sts_state);

      bool found_pkp_dynamic = transport_security_state->GetDynamicPKPState(
          domain, &dynamic_pkp_state);
      if (found_sts_dynamic) {
        result.SetInteger("dynamic_upgrade_mode",
                          static_cast<int>(dynamic_sts_state.upgrade_mode));
        result.SetBoolean("dynamic_sts_include_subdomains",
                          dynamic_sts_state.include_subdomains);
        result.SetDouble("dynamic_sts_observed",
                         dynamic_sts_state.last_observed.ToDoubleT());
        result.SetDouble("dynamic_sts_expiry",
                         dynamic_sts_state.expiry.ToDoubleT());
        result.SetString("dynamic_sts_domain", dynamic_sts_state.domain);
      }

      if (found_pkp_dynamic) {
        result.SetBoolean("dynamic_pkp_include_subdomains",
                          dynamic_pkp_state.include_subdomains);
        result.SetDouble("dynamic_pkp_observed",
                         dynamic_pkp_state.last_observed.ToDoubleT());
        result.SetDouble("dynamic_pkp_expiry",
                         dynamic_pkp_state.expiry.ToDoubleT());
        result.SetString("dynamic_spki_hashes",
                         HashesToBase64String(dynamic_pkp_state.spki_hashes));
        result.SetString("dynamic_pkp_domain", dynamic_pkp_state.domain);
      }

      result.SetBoolean("result",
                        found_static || found_sts_dynamic || found_pkp_dynamic);
    } else {
      result.SetString("error", "no TransportSecurityState active");
    }
  } else {
    result.SetString("error", "non-ASCII domain name");
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

void NetworkContext::PreconnectSockets(
    uint32_t num_streams,
    const GURL& original_url,
    bool allow_credentials,
    const net::NetworkIsolationKey& network_isolation_key) {
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
  request_info.network_isolation_key = network_isolation_key;

  net::HttpTransactionFactory* factory =
      url_request_context_->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();
  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  http_stream_factory->PreconnectStreams(
      base::saturated_cast<int32_t>(num_streams), request_info);
}

void NetworkContext::CreateP2PSocketManager(
    const net::NetworkIsolationKey& network_isolation_key,
    mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client,
    mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
        trusted_socket_manager,
    mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver) {
  std::unique_ptr<P2PSocketManager> socket_manager =
      std::make_unique<P2PSocketManager>(
          network_isolation_key, std::move(client),
          std::move(trusted_socket_manager), std::move(socket_manager_receiver),
          base::BindRepeating(&NetworkContext::DestroySocketManager,
                              base::Unretained(this)),
          url_request_context_);
  socket_managers_[socket_manager.get()] = std::move(socket_manager);
}

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
    const GURL& origin,
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

void NetworkContext::SetSplitAuthCacheByNetworkIsolationKey(
    bool split_auth_cache_by_network_isolation_key) {
  url_request_context_->http_transaction_factory()
      ->GetSession()
      ->http_auth_cache()
      ->SetKeyServerEntriesByNetworkIsolationKey(
          split_auth_cache_by_network_isolation_key);
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
    const net::NetworkIsolationKey& network_isolation_key,
    const net::AuthCredentials& credentials,
    AddAuthCacheEntryCallback callback) {
  if (challenge.challenger.scheme() == url::kFtpScheme) {
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    net::FtpAuthCache* auth_cache = url_request_context_->ftp_auth_cache();
    auth_cache->Add(challenge.challenger.GetURL(), credentials);
#else
    NOTREACHED();
#endif  // BUILDFLAG(DISABLE_FTP_SUPPORT)
  } else {
    net::HttpAuthCache* http_auth_cache =
        url_request_context_->http_transaction_factory()
            ->GetSession()
            ->http_auth_cache();
    http_auth_cache->Add(challenge.challenger.GetURL(),
                         challenge.is_proxy ? net::HttpAuth::AUTH_PROXY
                                            : net::HttpAuth::AUTH_SERVER,
                         challenge.realm,
                         net::HttpAuth::StringToScheme(challenge.scheme),
                         network_isolation_key, challenge.challenge,
                         credentials, challenge.path);
  }
  std::move(callback).Run();
}

void NetworkContext::LookupServerBasicAuthCredentials(
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    LookupServerBasicAuthCredentialsCallback callback) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  net::HttpAuthCache::Entry* entry =
      http_auth_cache->LookupByPath(url.GetOrigin(), net::HttpAuth::AUTH_SERVER,
                                    network_isolation_key, url.path());
  if (entry && entry->scheme() == net::HttpAuth::AUTH_SCHEME_BASIC)
    std::move(callback).Run(entry->credentials());
  else
    std::move(callback).Run(base::nullopt);
}

#if defined(OS_CHROMEOS)
void NetworkContext::LookupProxyAuthCredentials(
    const net::ProxyServer& proxy_server,
    const std::string& auth_scheme,
    const std::string& realm,
    LookupProxyAuthCredentialsCallback callback) {
  net::HttpAuth::Scheme net_scheme =
      net::HttpAuth::StringToScheme(base::ToLowerASCII(auth_scheme));
  if (net_scheme == net::HttpAuth::Scheme::AUTH_SCHEME_MAX) {
    std::move(callback).Run(base::nullopt);
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
  GURL proxy_url(scheme + proxy_server.host_port_pair().ToString());
  if (!proxy_url.is_valid()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  //  Unlike server credentials, proxy credentials are not keyed on
  //  NetworkIsolationKey.
  net::HttpAuthCache::Entry* entry =
      http_auth_cache->Lookup(proxy_url, net::HttpAuth::AUTH_PROXY, realm,
                              net_scheme, net::NetworkIsolationKey());
  if (entry)
    std::move(callback).Run(entry->credentials());
  else
    std::move(callback).Run(base::nullopt);
}
#endif

const net::HttpAuthPreferences* NetworkContext::GetHttpAuthPreferences() const {
  return &http_auth_merged_preferences_;
}

size_t NetworkContext::NumOpenQuicTransports() const {
  return std::count_if(quic_transports_.begin(), quic_transports_.end(),
                       [](const std::unique_ptr<QuicTransport>& transport) {
                         return !transport->torn_down();
                       });
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
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  http_auth_merged_preferences_.set_ntlm_v2_enabled(
      http_auth_dynamic_network_service_params->ntlm_v2_enabled);
#endif

#if defined(OS_ANDROID)
  http_auth_merged_preferences_.set_auth_android_negotiate_account_type(
      http_auth_dynamic_network_service_params->android_negotiate_account_type);
#endif

#if defined(OS_CHROMEOS)
  http_auth_merged_preferences_.set_allow_gssapi_library_load(
      http_auth_dynamic_network_service_params->allow_gssapi_library_load);
#endif
}

URLRequestContextOwner NetworkContext::MakeURLRequestContext(
    mojo::PendingRemote<mojom::URLLoaderFactory>
        url_loader_factory_for_cert_net_fetcher) {
  URLRequestContextBuilderMojo builder;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  DCHECK(
      g_cert_verifier_for_testing ||
      !base::FeatureList::IsEnabled(network::features::kCertVerifierService) ||
      (params_->cert_verifier_params &&
       params_->cert_verifier_params->is_remote_params()))
      << "If cert verification service is on, the creator of the "
         "NetworkContext should pass CertVerifierServiceRemoteParams.";

  std::unique_ptr<net::CertVerifier> cert_verifier;
  if (g_cert_verifier_for_testing) {
    cert_verifier = std::make_unique<WrappedTestingCertVerifier>();
  } else {
    if (params_->cert_verifier_params &&
        params_->cert_verifier_params->is_remote_params()) {
      // base::Unretained() is safe below because |this| will own
      // |cert_verifier|.
      // TODO(https://crbug.com/1085233): this cert verifier should deal with
      // disconnections if the CertVerifierService is run outside of the browser
      // process.
      cert_verifier = std::make_unique<cert_verifier::MojoCertVerifier>(
          std::move(params_->cert_verifier_params->get_remote_params()
                        ->cert_verifier_service),
          std::move(url_loader_factory_for_cert_net_fetcher),
          base::BindRepeating(
              &NetworkContext::CreateURLLoaderFactoryForCertNetFetcher,
              base::Unretained(this)));
    } else {
      mojom::CertVerifierCreationParams* creation_params = nullptr;
      if (params_->cert_verifier_params &&
          params_->cert_verifier_params->is_creation_params()) {
        creation_params =
            params_->cert_verifier_params->get_creation_params().get();
      }

      if (IsUsingCertNetFetcher())
        cert_net_fetcher_ =
            base::MakeRefCounted<net::CertNetFetcherURLRequest>();

      cert_verifier = CreateCertVerifier(creation_params, cert_net_fetcher_);
    }

    // Whether the cert verifier is remote or in-process, we should wrap it in
    // caching and coalescing layers to avoid extra verifications and IPCs.
    cert_verifier = std::make_unique<net::CachingCertVerifier>(
        std::make_unique<net::CoalescingCertVerifier>(
            std::move(cert_verifier)));

#if defined(OS_CHROMEOS)
    cert_verifier_with_trust_anchors_ =
        new CertVerifierWithTrustAnchors(base::BindRepeating(
            &NetworkContext::TrustAnchorUsed, base::Unretained(this)));
    UpdateAdditionalCertificates(
        std::move(params_->initial_additional_certificates));
    cert_verifier_with_trust_anchors_->InitializeOnIOThread(
        std::move(cert_verifier));
    cert_verifier = base::WrapUnique(cert_verifier_with_trust_anchors_);
#endif  // defined(OS_CHROMEOS)
  }

  builder.SetCertVerifier(IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
      *command_line, nullptr, std::move(cert_verifier)));

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
            std::move(params_->custom_proxy_config_client_receiver));
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

  scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store;
  if (params_->cookie_path) {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::ThreadTaskRunnerHandle::Get();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), net::GetCookieStoreBackgroundSequencePriority(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

    net::CookieCryptoDelegate* crypto_delegate = nullptr;
    if (params_->enable_encrypted_cookies) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !BUILDFLAG(IS_CHROMECAST)
      DCHECK(network_service_->os_crypt_config_set())
          << "NetworkService::SetCryptConfig must be called before creating a "
             "NetworkContext with encrypted cookies.";
#endif
      crypto_delegate = cookie_config::GetCookieCryptoDelegate();
    }
    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            params_->cookie_path.value(), client_task_runner,
            background_task_runner, params_->restore_old_session_cookies,
            crypto_delegate));

    session_cleanup_cookie_store =
        base::MakeRefCounted<SessionCleanupCookieStore>(sqlite_store);

    std::unique_ptr<net::CookieMonster> cookie_store =
        std::make_unique<net::CookieMonster>(session_cleanup_cookie_store.get(),
                                             net_log);
    if (params_->persist_session_cookies)
      cookie_store->SetPersistSessionCookies(true);

    builder.SetCookieStore(std::move(cookie_store));
  } else {
    DCHECK(!params_->restore_old_session_cookies);
    DCHECK(!params_->persist_session_cookies);
  }

  if (base::FeatureList::IsEnabled(features::kTrustTokens)) {
    trust_token_store_ = std::make_unique<PendingTrustTokenStore>();

    if (params_->trust_token_path) {
      SQLiteTrustTokenPersister::CreateForFilePath(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), kTrustTokenDatabaseTaskPriority,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          *params_->trust_token_path, kTrustTokenWriteBufferingWindow,
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
  if (params_->context_name)
    builder.set_name(*params_->context_name);

  if (params_->proxy_resolver_factory) {
    builder.SetMojoProxyResolverFactory(
        std::move(params_->proxy_resolver_factory));
  }

#if defined(OS_CHROMEOS)
  if (params_->dhcp_wpad_url_client) {
    builder.SetDhcpWpadUrlClient(std::move(params_->dhcp_wpad_url_client));
  }
#endif  // defined(OS_CHROMEOS)

  if (!params_->http_cache_enabled) {
    builder.DisableHttpCache();
  } else {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.max_size = params_->http_cache_max_size;
    if (!params_->http_cache_path) {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    } else {
      cache_params.path = *params_->http_cache_path;
      cache_params.type = network_session_configurator::ChooseCacheType();
    }
    cache_params.reset_cache = params_->reset_http_cache_backend;

#if defined(OS_ANDROID)
    cache_params.app_status_listener = app_status_listener();
#endif
    builder.EnableHttpCache(cache_params);
  }

  std::unique_ptr<SSLConfigServiceMojo> ssl_config_service =
      std::make_unique<SSLConfigServiceMojo>(
          std::move(params_->initial_ssl_config),
          std::move(params_->ssl_config_client_receiver),
          network_service_->crl_set_distributor(),
          network_service_->legacy_tls_config_distributor());
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
  if (params_->http_server_properties_path) {
    scoped_refptr<JsonPrefStore> json_pref_store(new JsonPrefStore(
        *params_->http_server_properties_path, nullptr,
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

  if (params_->transport_security_persister_path) {
    builder.set_transport_security_persister_path(
        *params_->transport_security_persister_path);
  }
  builder.set_hsts_policy_bypass_list(params_->hsts_policy_bypass_list);

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(params_->enable_ftp_url_support);
#else  // BUILDFLAG(DISABLE_FTP_SUPPORT)
  DCHECK(!params_->enable_ftp_url_support);
#endif

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

  if (params_->reporting_and_nel_store_path &&
      (reporting_enabled || nel_enabled)) {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::ThreadTaskRunnerHandle::Get();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(),
             net::GetReportingAndNelStoreBackgroundSequencePriority(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    std::unique_ptr<net::SQLitePersistentReportingAndNelStore> sqlite_store(
        new net::SQLitePersistentReportingAndNelStore(
            params_->reporting_and_nel_store_path.value(), client_task_runner,
            background_task_runner));
    builder.set_persistent_reporting_and_nel_store(std::move(sqlite_store));
  } else {
    builder.set_persistent_reporting_and_nel_store(nullptr);
  }

#endif  // BUILDFLAG(ENABLE_REPORTING)

  net::HttpNetworkSession::Params session_params;
  bool is_quic_force_disabled = false;
  if (network_service_ && network_service_->quic_disabled())
    is_quic_force_disabled = true;

  auto quic_context = std::make_unique<net::QuicContext>();
  network_session_configurator::ParseCommandLineAndFieldTrials(
      *base::CommandLine::ForCurrentProcess(), is_quic_force_disabled,
      params_->quic_user_agent_id, &session_params, quic_context->params());

  session_params.disable_idle_sockets_close_on_memory_pressure =
      params_->disable_idle_sockets_close_on_memory_pressure;

  if (network_service_) {
    session_params.key_auth_cache_server_entries_by_network_isolation_key =
        network_service_->split_auth_cache_by_network_isolation_key();
  }

  session_params.key_auth_cache_server_entries_by_network_isolation_key =
      base::FeatureList::IsEnabled(
          features::kSplitAuthCacheByNetworkIsolationKey);

  builder.set_http_network_session_params(session_params);
  builder.set_quic_context(std::move(quic_context));

  builder.SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](net::HttpNetworkSession* session)
                         -> std::unique_ptr<net::HttpTransactionFactory> {
        return std::make_unique<ThrottlingNetworkTransactionFactory>(session);
      }));

#if BUILDFLAG(IS_CT_SUPPORTED)
  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
  std::vector<std::pair<std::string, base::TimeDelta>> disqualified_logs;
  std::vector<std::string> operated_by_google_logs;

  if (!params_->ct_logs.empty()) {
    for (const auto& log : params_->ct_logs) {
      if (log->operated_by_google || log->disqualified_at) {
        std::string log_id = crypto::SHA256HashString(log->public_key);
        if (log->operated_by_google)
          operated_by_google_logs.push_back(log_id);
        if (log->disqualified_at) {
          disqualified_logs.push_back(
              std::make_pair(log_id, log->disqualified_at.value()));
        }
      }
      scoped_refptr<const net::CTLogVerifier> log_verifier =
          net::CTLogVerifier::Create(log->public_key, log->name);
      if (!log_verifier) {
        // TODO: Signal bad configuration (such as bad key).
        continue;
      }
      ct_logs.push_back(std::move(log_verifier));
    }
    auto ct_verifier = std::make_unique<net::MultiLogCTVerifier>();
    ct_verifier->AddLogs(ct_logs);
    builder.set_ct_verifier(std::move(ct_verifier));
  }

  if (params_->enforce_chrome_ct_policy) {
    std::sort(std::begin(operated_by_google_logs),
              std::end(operated_by_google_logs));
    std::sort(std::begin(disqualified_logs), std::end(disqualified_logs));

    builder.set_ct_policy_enforcer(
        std::make_unique<certificate_transparency::ChromeCTPolicyEnforcer>(
            params_->ct_log_update_time, disqualified_logs,
            operated_by_google_logs));
  }

  builder.set_sct_auditing_delegate(
      std::make_unique<SCTAuditingDelegate>(weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  builder.set_host_mapping_rules(
      command_line->GetSwitchValueASCII(switches::kHostResolverRules));

  auto result =
      URLRequestContextOwner(std::move(pref_service), builder.Build());

  result.url_request_context->set_require_network_isolation_key(
      params_->require_network_isolation_key);

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

#if defined(OS_ANDROID)
  result.url_request_context->set_check_cleartext_permitted(
      params_->check_clear_text_permitted);
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CT_SUPPORTED)
  if (params_->enable_expect_ct_reporting) {
    LazyCreateExpectCTReporter(result.url_request_context.get());
    result.url_request_context->transport_security_state()->SetExpectCTReporter(
        expect_ct_reporter_.get());
  }

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
  }

  cookie_manager_ = std::make_unique<CookieManager>(
      result.url_request_context->cookie_store(),
      std::move(session_cleanup_cookie_store),
      std::move(params_->cookie_manager_params));

  if (cert_net_fetcher_)
    cert_net_fetcher_->SetURLRequestContext(result.url_request_context.get());

  return result;
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

  url::Replacements<char> replacements;
  const char kNewScheme[] = "https";
  replacements.SetScheme(kNewScheme, url::Component(0, strlen(kNewScheme)));
  return original_url.ReplaceComponents(replacements);
}

void NetworkContext::DestroySocketManager(P2PSocketManager* socket_manager) {
  auto iter = socket_managers_.find(socket_manager);
  DCHECK(iter != socket_managers_.end());
  socket_managers_.erase(iter);
}

void NetworkContext::CanUploadDomainReliability(
    const GURL& origin,
    base::OnceCallback<void(bool)> callback) {
  client_->OnCanSendDomainReliabilityUpload(
      origin,
      base::BindOnce([](base::OnceCallback<void(bool)> callback,
                        bool allowed) { std::move(callback).Run(allowed); },
                     std::move(callback)));
}

void NetworkContext::OnCertVerifyForSignedExchangeComplete(int cert_verify_id,
                                                           int result) {
  auto iter = cert_verifier_requests_.find(cert_verify_id);
  DCHECK(iter != cert_verifier_requests_.end());

  auto pending_cert_verify = std::move(iter->second);
  cert_verifier_requests_.erase(iter);

  net::ct::CTVerifyResult ct_verify_result;
#if BUILDFLAG(IS_CT_SUPPORTED)
  if (result == net::OK) {
    net::X509Certificate* verified_cert =
        pending_cert_verify->result->verified_cert.get();
    url_request_context_->cert_transparency_verifier()->Verify(
        pending_cert_verify->url.host(), verified_cert,
        pending_cert_verify->ocsp_result, pending_cert_verify->sct_list,
        &ct_verify_result.scts,
        net::NetLogWithSource::Make(
            network_service_ ? url_request_context_->net_log() : nullptr,
            net::NetLogSourceType::CERT_VERIFIER_JOB));

    net::ct::SCTList verified_scts = net::ct::SCTsMatchingStatus(
        ct_verify_result.scts, net::ct::SCT_STATUS_OK);

    ct_verify_result.policy_compliance =
        url_request_context_->ct_policy_enforcer()->CheckCompliance(
            verified_cert, verified_scts,
            net::NetLogWithSource::Make(
                network_service_ ? url_request_context_->net_log() : nullptr,
                net::NetLogSourceType::CERT_VERIFIER_JOB));

    // TODO(https://crbug.com/803774): We should determine whether EV & SXG
    // should be a thing (due to the online/offline signing difference)
    if (pending_cert_verify->result->cert_status & net::CERT_STATUS_IS_EV &&
        ct_verify_result.policy_compliance !=
            net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS &&
        ct_verify_result.policy_compliance !=
            net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
      pending_cert_verify->result->cert_status |=
          net::CERT_STATUS_CT_COMPLIANCE_FAILED;
      pending_cert_verify->result->cert_status &= ~net::CERT_STATUS_IS_EV;
    }

    // TODO(https://crbug.com/1087091): Update
    // NetworkContext::VerifyCertForSignedExchange() to take a
    // NetworkIsolationKey, and pass it in here.
    net::TransportSecurityState::CTRequirementsStatus ct_requirement_status =
        url_request_context_->transport_security_state()->CheckCTRequirements(
            net::HostPortPair::FromURL(pending_cert_verify->url),
            pending_cert_verify->result->is_issued_by_known_root,
            pending_cert_verify->result->public_key_hashes, verified_cert,
            pending_cert_verify->certificate.get(), ct_verify_result.scts,
            net::TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct_verify_result.policy_compliance,
            net::NetworkIsolationKey::Todo());

    if (url_request_context_->sct_auditing_delegate() &&
        url_request_context_->sct_auditing_delegate()->IsSCTAuditingEnabled() &&
        pending_cert_verify->result->is_issued_by_known_root) {
      url_request_context_->sct_auditing_delegate()->MaybeEnqueueReport(
          net::HostPortPair::FromURL(pending_cert_verify->url), verified_cert,
          ct_verify_result.scts);
    }

    switch (ct_requirement_status) {
      case net::TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
        result = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
        break;
      case net::TransportSecurityState::CT_REQUIREMENTS_MET:
        ct_verify_result.policy_compliance_required = true;
        break;
      case net::TransportSecurityState::CT_NOT_REQUIRED:
        // CT is not required if the certificate does not chain to a publicly
        // trusted root certificate.
        if (!pending_cert_verify->result->is_issued_by_known_root) {
          ct_verify_result.policy_compliance_required = false;
          break;
        }
        // For old certificates (issued before 2018-05-01),
        // CheckCTRequirements() may return CT_NOT_REQUIRED, so we check the
        // compliance status here.
        // TODO(https://crbug.com/851778): Remove this condition once we require
        // signing certificates to have CanSignHttpExchanges extension, because
        // such certificates should be naturally after 2018-05-01.
        if (ct_verify_result.policy_compliance ==
                net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
            ct_verify_result.policy_compliance ==
                net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
          ct_verify_result.policy_compliance_required = true;
          break;
        }
        // Require CT compliance, by overriding CT_NOT_REQUIRED and treat it as
        // ERR_CERTIFICATE_TRANSPARENCY_REQUIRED.
        result = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

  std::move(pending_cert_verify->callback)
      .Run(result, *pending_cert_verify->result.get(), ct_verify_result);
}

#if defined(OS_CHROMEOS)
void NetworkContext::TrustAnchorUsed() {
  client_->OnTrustAnchorUsed();
}
#endif

void NetworkContext::InitializeCorsParams() {
  for (const auto& pattern : params_->cors_origin_access_list) {
    cors_origin_access_list_.SetAllowListForOrigin(pattern->source_origin,
                                                   pattern->allow_patterns);
    cors_origin_access_list_.SetBlockListForOrigin(pattern->source_origin,
                                                   pattern->block_patterns);
  }
  for (const auto& key : params_->cors_exempt_header_list)
    cors_exempt_header_list_.insert(key);
}

void NetworkContext::FinishConstructingTrustTokenStore(
    std::unique_ptr<SQLiteTrustTokenPersister> persister) {
  trust_token_store_->OnStoreReady(std::make_unique<TrustTokenStore>(
      std::move(persister),
      std::make_unique<ExpiryInspectingRecordExpiryDelegate>(
          network_service()->trust_token_key_commitments())));
}

void NetworkContext::GetOriginPolicyManager(
    mojo::PendingReceiver<mojom::OriginPolicyManager> receiver) {
  origin_policy_manager_->AddReceiver(std::move(receiver));
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

}  // namespace network
