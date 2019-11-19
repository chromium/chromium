// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_url_request_context_getter.h"

#include <utility>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#import "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/browsing_data/system_cookie_store_util.h"
#import "ios/web/public/web_client.h"
#include "net/base/cache_type.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/proxy_config_service_ios.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewURLRequestContextGetter::WebViewURLRequestContextGetter(
    const base::FilePath& base_path,
    web::BrowserState* browser_state,
    net::NetLog* net_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : base_path_(base_path),
      net_log_(net_log),
      network_task_runner_(network_task_runner),
      proxy_config_service_(
          new net::ProxyConfigServiceIOS(NO_TRAFFIC_ANNOTATION_YET)),
      system_cookie_store_(web::CreateSystemCookieStore(browser_state)),
      is_shutting_down_(false) {}

WebViewURLRequestContextGetter::~WebViewURLRequestContextGetter() = default;

net::URLRequestContext* WebViewURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (is_shutting_down_) {
    return nullptr;
  }

  if (!url_request_context_) {
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_net_log(net_log_);
    DCHECK(!network_delegate_.get());
    network_delegate_ = std::make_unique<net::NetworkDelegateImpl>();
    url_request_context_->set_network_delegate(network_delegate_.get());

    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));
    // Using std::move on a |system_cookie_store_| resets it to null as it's a
    // unique_ptr, so |system_cookie_store_| will not be a dangling pointer.
    storage_->set_cookie_store(std::make_unique<net::CookieStoreIOS>(
        std::move(system_cookie_store_), net_log_));

    web::WebClient* web_client = web::GetWebClient();
    DCHECK(web_client);
    std::string user_agent =
        web_client->GetUserAgent(web::UserAgentType::MOBILE);

    storage_->set_http_user_agent_settings(
        std::make_unique<net::StaticHttpUserAgentSettings>("en-us,en",
                                                           user_agent));
    storage_->set_proxy_resolution_service(
        net::ProxyResolutionService::CreateUsingSystemProxyResolver(
            std::move(proxy_config_service_), url_request_context_->net_log()));
    storage_->set_ssl_config_service(
        std::make_unique<net::SSLConfigServiceDefaults>());
    storage_->set_cert_verifier(
        net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr));

    storage_->set_transport_security_state(
        std::make_unique<net::TransportSecurityState>());
    storage_->set_cert_transparency_verifier(
        base::WrapUnique(new net::MultiLogCTVerifier));
    storage_->set_ct_policy_enforcer(
        base::WrapUnique(new net::DefaultCTPolicyEnforcer));
    storage_->set_quic_context(std::make_unique<net::QuicContext>());
    transport_security_persister_ =
        std::make_unique<net::TransportSecurityPersister>(
            url_request_context_->transport_security_state(), base_path_,
            base::CreateSequencedTaskRunner({base::ThreadPool(),
                                             base::MayBlock(),
                                             base::TaskPriority::BEST_EFFORT}));

    storage_->set_http_server_properties(
        std::make_unique<net::HttpServerProperties>());

    std::unique_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateStandaloneResolver(
            url_request_context_->net_log()));
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault());
    storage_->set_host_resolver(std::move(host_resolver));

    net::HttpNetworkSession::Context network_session_context;
    network_session_context.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_context.transport_security_state =
        url_request_context_->transport_security_state();
    network_session_context.cert_transparency_verifier =
        url_request_context_->cert_transparency_verifier();
    network_session_context.net_log = url_request_context_->net_log();
    network_session_context.proxy_resolution_service =
        url_request_context_->proxy_resolution_service();
    network_session_context.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_context.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_context.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_context.host_resolver =
        url_request_context_->host_resolver();
    network_session_context.ct_policy_enforcer =
        url_request_context_->ct_policy_enforcer();
    network_session_context.quic_context = url_request_context_->quic_context();

    base::FilePath cache_path =
        base_path_.Append(FILE_PATH_LITERAL("ChromeWebViewCache"));
    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, cache_path, 0));

    storage_->set_http_network_session(
        std::make_unique<net::HttpNetworkSession>(
            net::HttpNetworkSession::Params(), network_session_context));
    storage_->set_http_transaction_factory(std::make_unique<net::HttpCache>(
        storage_->http_network_session(), std::move(main_backend),
        true /* set_up_quic_server_info */));

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());

    storage_->set_job_factory(std::move(job_factory));
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebViewURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void WebViewURLRequestContextGetter::ShutDown() {
  is_shutting_down_ = true;
  net::URLRequestContextGetter::NotifyContextShuttingDown();
}

}  // namespace ios_web_view
