// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_session_pool.h"
#include "net/shared_dictionary/shared_dictionary_network_transaction_factory.h"
#include "net/socket/network_binding_client_socket_factory.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
#include "net/device_bound_sessions/session_service.h"
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

namespace net {

URLRequestContextBuilder::HttpCacheParams::HttpCacheParams() = default;
URLRequestContextBuilder::HttpCacheParams::~HttpCacheParams() = default;

URLRequestContextBuilder::URLRequestContextBuilder() = default;

URLRequestContextBuilder::~URLRequestContextBuilder() = default;

void URLRequestContextBuilder::SetHttpNetworkSessionComponents(
    const URLRequestContext* request_context,
    HttpNetworkSessionContext* session_context,
    bool suppress_setting_socket_performance_watcher_factory,
    ClientSocketFactory* client_socket_factory) {
  session_context->client_socket_factory =
      client_socket_factory ? client_socket_factory
                            : ClientSocketFactory::GetDefaultFactory();
  session_context->host_resolver = request_context->host_resolver();
  session_context->cert_verifier = request_context->cert_verifier();
  session_context->transport_security_state =
      request_context->transport_security_state();
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
  if (request_context->network_quality_estimator() &&
      !suppress_setting_socket_performance_watcher_factory) {
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

void URLRequestContextBuilder::set_reporting_service(
    std::unique_ptr<ReportingService> reporting_service) {
  reporting_service_ = std::move(reporting_service);
}

void URLRequestContextBuilder::set_persistent_reporting_and_nel_store(
    std::unique_ptr<PersistentReportingAndNelStore>
        persistent_reporting_and_nel_store) {
  persistent_reporting_and_nel_store_ =
      std::move(persistent_reporting_and_nel_store);
}

void URLRequestContextBuilder::set_enterprise_reporting_endpoints(
    const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints) {
  enterprise_reporting_endpoints_ = enterprise_reporting_endpoints;
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
  http_transaction_factory_.reset();
  create_http_network_transaction_factory_ =
      std::move(create_http_network_transaction_factory);
}

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
void URLRequestContextBuilder::set_device_bound_session_service(
    std::unique_ptr<device_bound_sessions::SessionService>
        device_bound_session_service) {
  device_bound_session_service_ = std::move(device_bound_session_service);
}
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

void URLRequestContextBuilder::BindToNetwork(
    handles::NetworkHandle network,
    std::optional<HostResolver::ManagerOptions> options) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(NetworkChangeNotifier::AreNetworkHandlesSupported());
  // DNS lookups for this context will need to target `network`. NDK to do that
  // has been introduced in Android Marshmallow
  // (https://developer.android.com/ndk/reference/group/networking#android_getaddrinfofornetwork)
  // This is also checked later on in the codepath (at lookup time), but
  // failing here should be preferred to return a more intuitive crash path.
  CHECK(base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_MARSHMALLOW);
  bound_network_ = network;
  manager_options_ = options.value_or(manager_options_);
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<URLRequestContext> URLRequestContextBuilder::Build() {
  auto context = std::make_unique<URLRequestContext>(
      base::PassKey<URLRequestContextBuilder>());

  context->set_enable_brotli(enable_brotli_);
  context->set_enable_zstd(enable_zstd_);
  context->set_check_cleartext_permitted(check_cleartext_permitted_);
  context->set_require_network_anonymization_key(
      require_network_anonymization_key_);
  context->set_network_quality_estimator(network_quality_estimator_);

  if (http_user_agent_settings_) {
    context->set_http_user_agent_settings(std::move(http_user_agent_settings_));
  } else {
    context->set_http_user_agent_settings(
        std::make_unique<StaticHttpUserAgentSettings>(accept_language_,
                                                      user_agent_));
  }

  if (!network_delegate_) {
    network_delegate_ = std::make_unique<NetworkDelegateImpl>();
  }
  context->set_network_delegate(std::move(network_delegate_));

  if (net_log_) {
    // Unlike the other builder parameters, |net_log_| is not owned by the
    // builder or resulting context.
    context->set_net_log(net_log_);
  } else {
    context->set_net_log(NetLog::Get());
  }

  if (bound_network_ != handles::kInvalidNetworkHandle) {
    DCHECK(!client_socket_factory_raw_);
    DCHECK(!host_resolver_);
    DCHECK(!host_resolver_manager_);
    DCHECK(!host_resolver_factory_);

    context->set_bound_network(bound_network_);

    // All sockets created for this context will need to be bound to
    // `bound_network_`.
    auto client_socket_factory =
        std::make_unique<NetworkBindingClientSocketFactory>(bound_network_);
    set_client_socket_factory(client_socket_factory.get());
    context->set_client_socket_factory(std::move(client_socket_factory));

    host_resolver_ = HostResolver::CreateStandaloneNetworkBoundResolver(
        context->net_log(), bound_network_, manager_options_);

    if (!quic_context_) {
      set_quic_context(std::make_unique<QuicContext>());
    }
    auto* quic_params = quic_context_->params();
    // QUIC sessions for this context should not be closed (or go away) after a
    // network change.
    quic_params->close_sessions_on_ip_change = false;
    quic_params->goaway_sessions_on_ip_change = false;

    // QUIC connection migration should not be enabled when binding a context
    // to a network.
    quic_params->migrate_sessions_on_network_change_v2 = false;

    // Objects used by network sessions for this context shouldn't listen to
    // network changes.
    http_network_session_params_.ignore_ip_address_changes = true;
  }

  if (client_socket_factory_) {
    context->set_client_socket_factory(std::move(client_socket_factory_));
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
  context->set_host_resolver(std::move(host_resolver_));

  if (ssl_config_service_) {
    context->set_ssl_config_service(std::move(ssl_config_service_));
  } else {
    context->set_ssl_config_service(
        std::make_unique<SSLConfigServiceDefaults>());
  }

  if (http_auth_handler_factory_) {
    context->set_http_auth_handler_factory(
        std::move(http_auth_handler_factory_));
  } else {
    context->set_http_auth_handler_factory(
        HttpAuthHandlerRegistryFactory::CreateDefault());
  }

  if (cookie_store_set_by_client_) {
    context->set_cookie_store(std::move(cookie_store_));
  } else {
    auto cookie_store = std::make_unique<CookieMonster>(nullptr /* store */,
                                                        context->net_log());
    context->set_cookie_store(std::move(cookie_store));
  }

  context->set_transport_security_state(
      std::make_unique<TransportSecurityState>(hsts_policy_bypass_list_));
  if (!transport_security_persister_file_path_.empty()) {
    // Use a low priority because saving this should not block anything
    // user-visible. Block shutdown to ensure it does get persisted to disk,
    // since it contains security-relevant information.
    scoped_refptr<base::SequencedTaskRunner> task_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

    context->set_transport_security_persister(
        std::make_unique<TransportSecurityPersister>(
            context->transport_security_state(), task_runner,
            transport_security_persister_file_path_));
  }

  if (http_server_properties_) {
    context->set_http_server_properties(std::move(http_server_properties_));
  } else {
    context->set_http_server_properties(
        std::make_unique<HttpServerProperties>());
  }

  if (cert_verifier_) {
    context->set_cert_verifier(std::move(cert_verifier_));
  } else {
    // TODO(mattm): Should URLRequestContextBuilder create a CertNetFetcher?
    context->set_cert_verifier(
        CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr));
  }

  if (sct_auditing_delegate_) {
    context->set_sct_auditing_delegate(std::move(sct_auditing_delegate_));
  }

  if (quic_context_) {
    context->set_quic_context(std::move(quic_context_));
  } else {
    context->set_quic_context(std::make_unique<QuicContext>());
  }

  if (!proxy_resolution_service_) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
    // TODO(willchan): Switch to using this code when
    // ProxyConfigService::CreateSystemProxyConfigService()'s
    // signature doesn't suck.
    if (!proxy_config_service_) {
      proxy_config_service_ =
          ProxyConfigService::CreateSystemProxyConfigService(
              base::SingleThreadTaskRunner::GetCurrentDefault().get());
    }
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) &&
        // !BUILDFLAG(IS_ANDROID)
    proxy_resolution_service_ = CreateProxyResolutionService(
        std::move(proxy_config_service_), context.get(),
        context->host_resolver(), context->network_delegate(),
        context->net_log(), pac_quick_check_enabled_);
  }
  ProxyResolutionService* proxy_resolution_service =
      proxy_resolution_service_.get();
  context->set_proxy_resolution_service(std::move(proxy_resolution_service_));

  if (proxy_delegate_) {
    ProxyDelegate* proxy_delegate = proxy_delegate_.get();
    context->set_proxy_delegate(std::move(proxy_delegate_));

    proxy_resolution_service->SetProxyDelegate(proxy_delegate);
    proxy_delegate->SetProxyResolutionService(proxy_resolution_service);
  }

#if BUILDFLAG(ENABLE_REPORTING)
  // Note: ReportingService::Create and NetworkErrorLoggingService::Create can
  // both return nullptr if the corresponding base::Feature is disabled.

  if (reporting_service_) {
    context->set_reporting_service(std::move(reporting_service_));
  } else if (reporting_policy_) {
    context->set_reporting_service(
        ReportingService::Create(*reporting_policy_, context.get(),
                                 persistent_reporting_and_nel_store_.get(),
                                 enterprise_reporting_endpoints_));
  }

  if (network_error_logging_enabled_) {
    if (!network_error_logging_service_) {
      network_error_logging_service_ = NetworkErrorLoggingService::Create(
          persistent_reporting_and_nel_store_.get());
    }
    context->set_network_error_logging_service(
        std::move(network_error_logging_service_));
  }

  if (persistent_reporting_and_nel_store_) {
    context->set_persistent_reporting_and_nel_store(
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

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  if (has_device_bound_session_service_) {
    context->set_device_bound_session_service(
        device_bound_sessions::SessionService::Create(context.get()));
  } else {
    if (device_bound_session_service_) {
      context->set_device_bound_session_service(
          std::move(device_bound_session_service_));
    }
  }
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

  HttpNetworkSessionContext network_session_context;
  // Unlike the other fields of HttpNetworkSession::Context,
  // |client_socket_factory| is not mirrored in URLRequestContext.
  SetHttpNetworkSessionComponents(
      context.get(), &network_session_context,
      suppress_setting_socket_performance_watcher_factory_for_testing_,
      client_socket_factory_raw_);

  context->set_http_network_session(std::make_unique<HttpNetworkSession>(
      http_network_session_params_, network_session_context));

  std::unique_ptr<HttpTransactionFactory> http_transaction_factory;
  if (http_transaction_factory_) {
    http_transaction_factory = std::move(http_transaction_factory_);
  } else if (!create_http_network_transaction_factory_.is_null()) {
    http_transaction_factory =
        std::move(create_http_network_transaction_factory_)
            .Run(context->http_network_session());
  } else {
    http_transaction_factory =
        std::make_unique<HttpNetworkLayer>(context->http_network_session());
  }

  if (enable_shared_dictionary_) {
    http_transaction_factory =
        std::make_unique<SharedDictionaryNetworkTransactionFactory>(
            std::move(http_transaction_factory), enable_shared_zstd_);
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
          NOTREACHED_IN_MIGRATION();
          break;
      }
      http_cache_backend = std::make_unique<HttpCache::DefaultBackend>(
          DISK_CACHE, backend_type, http_cache_params_.file_operations_factory,
          http_cache_params_.path, http_cache_params_.max_size,
          http_cache_params_.reset_cache);
    } else {
      http_cache_backend =
          HttpCache::DefaultBackend::InMemory(http_cache_params_.max_size);
    }
#if BUILDFLAG(IS_ANDROID)
    http_cache_backend->SetAppStatusListenerGetter(
        http_cache_params_.app_status_listener_getter);
#endif

    http_transaction_factory = std::make_unique<HttpCache>(
        std::move(http_transaction_factory), std::move(http_cache_backend));
  }
  context->set_http_transaction_factory(std::move(http_transaction_factory));

  std::unique_ptr<URLRequestJobFactory> job_factory =
      std::make_unique<URLRequestJobFactory>();
  for (auto& scheme_handler : protocol_handlers_) {
    job_factory->SetProtocolHandler(scheme_handler.first,
                                    std::move(scheme_handler.second));
  }
  protocol_handlers_.clear();

  context->set_job_factory(std::move(job_factory));

  if (cookie_deprecation_label_.has_value()) {
    context->set_cookie_deprecation_label(*cookie_deprecation_label_);
  }

  return context;
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
