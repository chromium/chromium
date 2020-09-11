// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_stream_factory.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "url/url_constants.h"

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
#include "net/ftp/ftp_auth_cache.h"                // nogncheck
#include "net/ftp/ftp_network_layer.h"             // nogncheck
#include "net/url_request/ftp_protocol_handler.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

namespace {

class BasicNetworkDelegate : public NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() = default;
  ~BasicNetworkDelegate() override = default;

 private:
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override {
    return OK;
  }

  int OnBeforeStartTransaction(URLRequest* request,
                               CompletionOnceCallback callback,
                               HttpRequestHeaders* headers) override {
    return OK;
  }

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& endpoint,
      base::Optional<GURL>* preserve_fragment_on_redirect_url) override {
    return OK;
  }

  void OnBeforeRedirect(URLRequest* request,
                        const GURL& new_location) override {}

  void OnResponseStarted(URLRequest* request, int net_error) override {}

  void OnCompleted(URLRequest* request, bool started, int net_error) override {}

  void OnURLRequestDestroyed(URLRequest* request) override {}

  void OnPACScriptError(int line_number, const base::string16& error) override {
  }

  bool OnCanGetCookies(const URLRequest& request,
                       bool allowed_from_caller) override {
    return allowed_from_caller;
  }

  bool OnCanSetCookie(const URLRequest& request,
                      const CanonicalCookie& cookie,
                      CookieOptions* options,
                      bool allowed_from_caller) override {
    return allowed_from_caller;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

// A URLRequestContext subclass that owns most of its components
// via a UrlRequestContextStorage object. When URLRequestContextBuilder::Build()
// is called, ownership of all URLRequestContext components is passed to the
// ContainerURLRequestContext. Since this cancels requests in its destructor,
// it's not safe to subclass this.
class ContainerURLRequestContext final : public URLRequestContext {
 public:
  explicit ContainerURLRequestContext() : storage_(this) {}

  ~ContainerURLRequestContext() override {
#if BUILDFLAG(ENABLE_REPORTING)
    // Shut down the NetworkErrorLoggingService so that destroying the
    // ReportingService (which might abort in-flight URLRequests, generating
    // network errors) won't recursively try to queue more network error
    // reports.
    if (network_error_logging_service())
      network_error_logging_service()->OnShutdown();

    // Shut down the ReportingService before the rest of the URLRequestContext,
    // so it cancels any pending requests it may have.
    if (reporting_service())
      reporting_service()->OnShutdown();
#endif  // BUILDFLAG(ENABLE_REPORTING)

    // Shut down the ProxyResolutionService, as it may have pending URLRequests
    // using this context. Since this cancels requests, it's not safe to
    // subclass this, as some parts of the URLRequestContext may then be torn
    // down before this cancels the ProxyResolutionService's URLRequests.
    proxy_resolution_service()->OnShutdown();

    DCHECK(host_resolver());
    host_resolver()->OnShutdown();

    AssertNoURLRequests();
  }

  URLRequestContextStorage* storage() { return &storage_; }

  void set_transport_security_persister(
      std::unique_ptr<TransportSecurityPersister>
          transport_security_persister) {
    transport_security_persister_ = std::move(transport_security_persister);
  }

 private:
  URLRequestContextStorage storage_;
  std::unique_ptr<TransportSecurityPersister> transport_security_persister_;

  DISALLOW_COPY_AND_ASSIGN(ContainerURLRequestContext);
};

}  // namespace

URLRequestContextBuilder::HttpCacheParams::HttpCacheParams()
    : type(IN_MEMORY), max_size(0), reset_cache(false) {}
URLRequestContextBuilder::HttpCacheParams::~HttpCacheParams() = default;

URLRequestContextBuilder::URLRequestContextBuilder() = default;

URLRequestContextBuilder::~URLRequestContextBuilder() = default;

void URLRequestContextBuilder::SetHttpNetworkSessionComponents(
    const URLRequestContext* request_context,
    HttpNetworkSession::Context* session_context) {
  session_context->host_resolver = request_context->host_resolver();
  session_context->cert_verifier = request_context->cert_verifier();
  session_context->transport_security_state =
      request_context->transport_security_state();
  session_context->cert_transparency_verifier =
      request_context->cert_transparency_verifier();
  session_context->ct_policy_enforcer = request_context->ct_policy_enforcer();
  session_context->sct_auditing_delegate =
      request_context->sct_auditing_delegate();
  session_context->proxy_resolution_service =
      request_context->proxy_resolution_service();
  session_context->proxy_delegate = request_context->proxy_delegate();
  session_context->http_user_agent_settings =
      request_context->http_user_agent_settings();
  session_context->ssl_config_service = request_context->ssl_config_service();
  session_context->http_auth_handler_factory =
      request_context->http_auth_handler_factory();
  session_context->http_server_properties =
      request_context->http_server_properties();
  session_context->quic_context = request_context->quic_context();
  session_context->net_log = request_context->net_log();
  session_context->network_quality_estimator =
      request_context->network_quality_estimator();
  if (request_context->network_quality_estimator()) {
    session_context->socket_performance_watcher_factory =
        request_context->network_quality_estimator()
            ->GetSocketPerformanceWatcherFactory();
  }
#if BUILDFLAG(ENABLE_REPORTING)
  session_context->reporting_service = request_context->reporting_service();
  session_context->network_error_logging_service =
      request_context->network_error_logging_service();
#endif
}

void URLRequestContextBuilder::set_accept_language(
    const std::string& accept_language) {
  DCHECK(!http_user_agent_settings_);
  accept_language_ = accept_language;
}
void URLRequestContextBuilder::set_user_agent(const std::string& user_agent) {
  DCHECK(!http_user_agent_settings_);
  user_agent_ = user_agent;
}

void URLRequestContextBuilder::set_http_user_agent_settings(
    std::unique_ptr<HttpUserAgentSettings> http_user_agent_settings) {
  http_user_agent_settings_ = std::move(http_user_agent_settings);
}

void URLRequestContextBuilder::EnableHttpCache(const HttpCacheParams& params) {
  http_cache_enabled_ = true;
  http_cache_params_ = params;
}

void URLRequestContextBuilder::DisableHttpCache() {
  http_cache_enabled_ = false;
  http_cache_params_ = HttpCacheParams();
}

void URLRequestContextBuilder::SetSpdyAndQuicEnabled(bool spdy_enabled,
                                                     bool quic_enabled) {
  http_network_session_params_.enable_http2 = spdy_enabled;
  http_network_session_params_.enable_quic = quic_enabled;
}

void URLRequestContextBuilder::set_ct_verifier(
    std::unique_ptr<CTVerifier> ct_verifier) {
  ct_verifier_ = std::move(ct_verifier);
}

void URLRequestContextBuilder::set_ct_policy_enforcer(
    std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer) {
  ct_policy_enforcer_ = std::move(ct_policy_enforcer);
}

void URLRequestContextBuilder::set_sct_auditing_delegate(
    std::unique_ptr<SCTAuditingDelegate> sct_auditing_delegate) {
  sct_auditing_delegate_ = std::move(sct_auditing_delegate);
}

void URLRequestContextBuilder::set_quic_context(
    std::unique_ptr<QuicContext> quic_context) {
  quic_context_ = std::move(quic_context);
}

void URLRequestContextBuilder::SetCertVerifier(
    std::unique_ptr<CertVerifier> cert_verifier) {
  cert_verifier_ = std::move(cert_verifier);
}

#if BUILDFLAG(ENABLE_REPORTING)
void URLRequestContextBuilder::set_reporting_policy(
    std::unique_ptr<ReportingPolicy> reporting_policy) {
  reporting_policy_ = std::move(reporting_policy);
}

void URLRequestContextBuilder::set_persistent_reporting_and_nel_store(
    std::unique_ptr<PersistentReportingAndNelStore>
        persistent_reporting_and_nel_store) {
  persistent_reporting_and_nel_store_ =
      std::move(persistent_reporting_and_nel_store);
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void URLRequestContextBuilder::SetCookieStore(
    std::unique_ptr<CookieStore> cookie_store) {
  cookie_store_set_by_client_ = true;
  cookie_store_ = std::move(cookie_store);
}

void URLRequestContextBuilder::SetProtocolHandler(
    const std::string& scheme,
    std::unique_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler) {
  DCHECK(protocol_handler);
  // If a consumer sets a ProtocolHandler and then overwrites it with another,
  // it's probably a bug.
  DCHECK_EQ(0u, protocol_handlers_.count(scheme));
  protocol_handlers_[scheme] = std::move(protocol_handler);
}

void URLRequestContextBuilder::set_host_resolver(
    std::unique_ptr<HostResolver> host_resolver) {
  DCHECK(!host_resolver_manager_);
  DCHECK(host_mapping_rules_.empty());
  DCHECK(!host_resolver_factory_);
  host_resolver_ = std::move(host_resolver);
}

void URLRequestContextBuilder::set_host_mapping_rules(
    std::string host_mapping_rules) {
  DCHECK(!host_resolver_);
  host_mapping_rules_ = std::move(host_mapping_rules);
}

void URLRequestContextBuilder::set_host_resolver_manager(
    HostResolverManager* manager) {
  DCHECK(!host_resolver_);
  host_resolver_manager_ = manager;
}

void URLRequestContextBuilder::set_host_resolver_factory(
    HostResolver::Factory* factory) {
  DCHECK(!host_resolver_);
  host_resolver_factory_ = factory;
}

void URLRequestContextBuilder::set_proxy_delegate(
    std::unique_ptr<ProxyDelegate> proxy_delegate) {
  proxy_delegate_ = std::move(proxy_delegate);
}

void URLRequestContextBuilder::SetHttpAuthHandlerFactory(
    std::unique_ptr<HttpAuthHandlerFactory> factory) {
  http_auth_handler_factory_ = std::move(factory);
}

void URLRequestContextBuilder::SetHttpServerProperties(
    std::unique_ptr<HttpServerProperties> http_server_properties) {
  http_server_properties_ = std::move(http_server_properties);
}

void URLRequestContextBuilder::SetCreateHttpTransactionFactoryCallback(
    CreateHttpTransactionFactoryCallback
        create_http_network_transaction_factory) {
  create_http_network_transaction_factory_ =
      std::move(create_http_network_transaction_factory);
}

std::unique_ptr<URLRequestContext> URLRequestContextBuilder::Build() {
  std::unique_ptr<ContainerURLRequestContext> context(
      new ContainerURLRequestContext());
  URLRequestContextStorage* storage = context->storage();

  if (!name_.empty())
    context->set_name(name_);
  context->set_enable_brotli(enable_brotli_);
  context->set_network_quality_estimator(network_quality_estimator_);

  if (http_user_agent_settings_) {
    storage->set_http_user_agent_settings(std::move(http_user_agent_settings_));
  } else {
    storage->set_http_user_agent_settings(
        std::make_unique<StaticHttpUserAgentSettings>(accept_language_,
                                                      user_agent_));
  }

  if (!network_delegate_)
    network_delegate_ = std::make_unique<BasicNetworkDelegate>();
  storage->set_network_delegate(std::move(network_delegate_));

  if (net_log_) {
    // Unlike the other builder parameters, |net_log_| is not owned by the
    // builder or resulting context.
    context->set_net_log(net_log_);
  } else {
    context->set_net_log(NetLog::Get());
  }

  if (host_resolver_) {
    DCHECK(host_mapping_rules_.empty());
    DCHECK(!host_resolver_manager_);
    DCHECK(!host_resolver_factory_);
  } else if (host_resolver_manager_) {
    if (host_resolver_factory_) {
      host_resolver_ = host_resolver_factory_->CreateResolver(
          host_resolver_manager_, host_mapping_rules_,
          true /* enable_caching */);
    } else {
      host_resolver_ = HostResolver::CreateResolver(host_resolver_manager_,
                                                    host_mapping_rules_,
                                                    true /* enable_caching */);
    }
  } else {
    if (host_resolver_factory_) {
      host_resolver_ = host_resolver_factory_->CreateStandaloneResolver(
          context->net_log(), HostResolver::ManagerOptions(),
          host_mapping_rules_, true /* enable_caching */);
    } else {
      host_resolver_ = HostResolver::CreateStandaloneResolver(
          context->net_log(), HostResolver::ManagerOptions(),
          host_mapping_rules_, true /* enable_caching */);
    }
  }
  host_resolver_->SetRequestContext(context.get());
  storage->set_host_resolver(std::move(host_resolver_));

  if (ssl_config_service_) {
    storage->set_ssl_config_service(std::move(ssl_config_service_));
  } else {
    storage->set_ssl_config_service(
        std::make_unique<SSLConfigServiceDefaults>());
  }

  if (http_auth_handler_factory_) {
    storage->set_http_auth_handler_factory(
        std::move(http_auth_handler_factory_));
  } else {
    storage->set_http_auth_handler_factory(
        HttpAuthHandlerRegistryFactory::CreateDefault());
  }

  if (cookie_store_set_by_client_) {
    storage->set_cookie_store(std::move(cookie_store_));
  } else {
    std::unique_ptr<CookieStore> cookie_store(
        new CookieMonster(nullptr /* store */, context->net_log()));
    storage->set_cookie_store(std::move(cookie_store));
  }

  storage->set_transport_security_state(
      std::make_unique<TransportSecurityState>(hsts_policy_bypass_list_));
  if (!transport_security_persister_path_.empty()) {
    // Use a low priority because saving this should not block anything
    // user-visible. Block shutdown to ensure it does get persisted to disk,
    // since it contains security-relevant information.
    scoped_refptr<base::SequencedTaskRunner> task_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

    context->set_transport_security_persister(
        std::make_unique<TransportSecurityPersister>(
            context->transport_security_state(),
            transport_security_persister_path_, task_runner));
  }

  if (http_server_properties_) {
    storage->set_http_server_properties(std::move(http_server_properties_));
  } else {
    storage->set_http_server_properties(
        std::make_unique<HttpServerProperties>());
  }

  if (cert_verifier_) {
    storage->set_cert_verifier(std::move(cert_verifier_));
  } else {
    // TODO(mattm): Should URLRequestContextBuilder create a CertNetFetcher?
    storage->set_cert_verifier(
        CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr));
  }

  if (ct_verifier_) {
    storage->set_cert_transparency_verifier(std::move(ct_verifier_));
  } else {
    storage->set_cert_transparency_verifier(
        std::make_unique<MultiLogCTVerifier>());
  }
  if (ct_policy_enforcer_) {
    storage->set_ct_policy_enforcer(std::move(ct_policy_enforcer_));
  } else {
    storage->set_ct_policy_enforcer(
        std::make_unique<DefaultCTPolicyEnforcer>());
  }

  if (sct_auditing_delegate_) {
    storage->set_sct_auditing_delegate(std::move(sct_auditing_delegate_));
  }

  if (quic_context_) {
    storage->set_quic_context(std::move(quic_context_));
  } else {
    storage->set_quic_context(std::make_unique<QuicContext>());
  }

  if (throttling_enabled_) {
    storage->set_throttler_manager(
        std::make_unique<URLRequestThrottlerManager>());
  }

  if (!proxy_resolution_service_) {
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // TODO(willchan): Switch to using this code when
    // ConfiguredProxyResolutionService::CreateSystemProxyConfigService()'s
    // signature doesn't suck.
    if (!proxy_config_service_) {
      proxy_config_service_ =
          ConfiguredProxyResolutionService::CreateSystemProxyConfigService(
              base::ThreadTaskRunnerHandle::Get().get());
    }
#endif  // !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    proxy_resolution_service_ = CreateProxyResolutionService(
        std::move(proxy_config_service_), context.get(),
        context->host_resolver(), context->network_delegate(),
        context->net_log(), pac_quick_check_enabled_);
  }
  ProxyResolutionService* proxy_resolution_service =
      proxy_resolution_service_.get();
  storage->set_proxy_resolution_service(std::move(proxy_resolution_service_));

#if BUILDFLAG(ENABLE_REPORTING)
  // Note: ReportingService::Create and NetworkErrorLoggingService::Create can
  // both return nullptr if the corresponding base::Feature is disabled.

  if (reporting_policy_) {
    storage->set_reporting_service(
        ReportingService::Create(*reporting_policy_, context.get(),
                                 persistent_reporting_and_nel_store_.get()));
  }

  if (network_error_logging_enabled_) {
    storage->set_network_error_logging_service(
        NetworkErrorLoggingService::Create(
            persistent_reporting_and_nel_store_.get()));
  }

  if (persistent_reporting_and_nel_store_) {
    storage->set_persistent_reporting_and_nel_store(
        std::move(persistent_reporting_and_nel_store_));
  }

  // If both Reporting and Network Error Logging are actually enabled, then
  // connect them so Network Error Logging can use Reporting to deliver error
  // reports.
  if (context->reporting_service() &&
      context->network_error_logging_service()) {
    context->network_error_logging_service()->SetReportingService(
        context->reporting_service());
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  if (proxy_delegate_) {
    proxy_resolution_service->SetProxyDelegate(proxy_delegate_.get());
    storage->set_proxy_delegate(std::move(proxy_delegate_));
  }

  HttpNetworkSession::Context network_session_context;
  SetHttpNetworkSessionComponents(context.get(), &network_session_context);

  storage->set_http_network_session(std::make_unique<HttpNetworkSession>(
      http_network_session_params_, network_session_context));

  std::unique_ptr<HttpTransactionFactory> http_transaction_factory;
  if (!create_http_network_transaction_factory_.is_null()) {
    http_transaction_factory =
        std::move(create_http_network_transaction_factory_)
            .Run(storage->http_network_session());
  } else {
    http_transaction_factory =
        std::make_unique<HttpNetworkLayer>(storage->http_network_session());
  }

  if (http_cache_enabled_) {
    std::unique_ptr<HttpCache::BackendFactory> http_cache_backend;
    if (http_cache_params_.type != HttpCacheParams::IN_MEMORY) {
      // TODO(mmenke): Maybe merge BackendType and HttpCacheParams::Type? The
      // first doesn't include in memory, so may require some work.
      BackendType backend_type = CACHE_BACKEND_DEFAULT;
      switch (http_cache_params_.type) {
        case HttpCacheParams::DISK:
          backend_type = CACHE_BACKEND_DEFAULT;
          break;
        case HttpCacheParams::DISK_BLOCKFILE:
          backend_type = CACHE_BACKEND_BLOCKFILE;
          break;
        case HttpCacheParams::DISK_SIMPLE:
          backend_type = CACHE_BACKEND_SIMPLE;
          break;
        case HttpCacheParams::IN_MEMORY:
          NOTREACHED();
          break;
      }
      http_cache_backend.reset(new HttpCache::DefaultBackend(
          DISK_CACHE, backend_type, http_cache_params_.path,
          http_cache_params_.max_size, http_cache_params_.reset_cache));
    } else {
      http_cache_backend =
          HttpCache::DefaultBackend::InMemory(http_cache_params_.max_size);
    }
#if defined(OS_ANDROID)
    http_cache_backend->SetAppStatusListener(
        http_cache_params_.app_status_listener);
#endif

    http_transaction_factory.reset(
        new HttpCache(std::move(http_transaction_factory),
                      std::move(http_cache_backend), true));
  }
  storage->set_http_transaction_factory(std::move(http_transaction_factory));

  std::unique_ptr<URLRequestJobFactory> job_factory =
      std::make_unique<URLRequestJobFactory>();
  // Adds caller-provided protocol handlers first so that these handlers are
  // used over the ftp handler below.
  for (auto& scheme_handler : protocol_handlers_) {
    job_factory->SetProtocolHandler(scheme_handler.first,
                                    std::move(scheme_handler.second));
  }
  protocol_handlers_.clear();

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  if (ftp_enabled_) {
    storage->set_ftp_auth_cache(std::make_unique<FtpAuthCache>());
    job_factory->SetProtocolHandler(
        url::kFtpScheme, FtpProtocolHandler::Create(context->host_resolver(),
                                                    context->ftp_auth_cache()));
  }
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  storage->set_job_factory(std::move(job_factory));

  return std::move(context);
}

std::unique_ptr<ProxyResolutionService>
URLRequestContextBuilder::CreateProxyResolutionService(
    std::unique_ptr<ProxyConfigService> proxy_config_service,
    URLRequestContext* url_request_context,
    HostResolver* host_resolver,
    NetworkDelegate* network_delegate,
    NetLog* net_log,
    bool pac_quick_check_enabled) {
  return ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
      std::move(proxy_config_service), net_log, pac_quick_check_enabled);
}

}  // namespace net
