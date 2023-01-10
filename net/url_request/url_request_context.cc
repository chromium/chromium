// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include <inttypes.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_delegate.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket_impl.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_throttler_manager.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

URLRequestContext::URLRequestContext(
    base::PassKey<URLRequestContextBuilder> pass_key)
    : url_requests_(std::make_unique<std::set<const URLRequest*>>()),
      bound_network_(handles::kInvalidNetworkHandle) {}

URLRequestContext::~URLRequestContext() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

const HttpNetworkSessionParams* URLRequestContext::GetNetworkSessionParams()
    const {
  HttpTransactionFactory* transaction_factory = http_transaction_factory();
  if (!transaction_factory)
    return nullptr;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  if (!network_session)
    return nullptr;
  return &network_session->params();
}

const HttpNetworkSessionContext* URLRequestContext::GetNetworkSessionContext()
    const {
  HttpTransactionFactory* transaction_factory = http_transaction_factory();
  if (!transaction_factory)
    return nullptr;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  if (!network_session)
    return nullptr;
  return &network_session->context();
}

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if !BUILDFLAG(IS_WIN) && \
    !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
std::unique_ptr<URLRequest> URLRequestContext::CreateRequest(
    const GURL& url,
    RequestPriority priority,
    URLRequest::Delegate* delegate) const {
  return CreateRequest(url, priority, delegate, MISSING_TRAFFIC_ANNOTATION,
                       /*is_for_websockets=*/false);
}
#endif

std::unique_ptr<URLRequest> URLRequestContext::CreateRequest(
    const GURL& url,
    RequestPriority priority,
    URLRequest::Delegate* delegate,
    NetworkTrafficAnnotationTag traffic_annotation,
    bool is_for_websockets,
    const absl::optional<net::NetLogSource> net_log_source) const {
  return std::make_unique<URLRequest>(
      base::PassKey<URLRequestContext>(), url, priority, delegate, this,
      traffic_annotation, is_for_websockets, net_log_source);
}

void URLRequestContext::AssertNoURLRequests() const {
  int num_requests = url_requests_->size();
  if (num_requests != 0) {
    // We're leaking URLRequests :( Dump the URL of the first one and record how
    // many we leaked so we have an idea of how bad it is.
    const URLRequest* request = *url_requests_->begin();
    int load_flags = request->load_flags();
    DEBUG_ALIAS_FOR_GURL(url_buf, request->url());
    base::debug::Alias(&num_requests);
    base::debug::Alias(&load_flags);
    CHECK(false) << "Leaked " << num_requests << " URLRequest(s). First URL: "
                 << request->url().spec().c_str() << ".";
  }
}

void URLRequestContext::set_net_log(NetLog* net_log) {
  net_log_ = net_log;
}
void URLRequestContext::set_host_resolver(
    std::unique_ptr<HostResolver> host_resolver) {
  DCHECK(host_resolver.get());
  host_resolver_ = std::move(host_resolver);
}
void URLRequestContext::set_cert_verifier(
    std::unique_ptr<CertVerifier> cert_verifier) {
  cert_verifier_ = std::move(cert_verifier);
}
void URLRequestContext::set_proxy_resolution_service(
    std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
  proxy_resolution_service_ = std::move(proxy_resolution_service);
}
void URLRequestContext::set_proxy_delegate(
    std::unique_ptr<ProxyDelegate> proxy_delegate) {
  proxy_delegate_ = std::move(proxy_delegate);
}
void URLRequestContext::set_ssl_config_service(
    std::unique_ptr<SSLConfigService> service) {
  ssl_config_service_ = std::move(service);
}
void URLRequestContext::set_http_auth_handler_factory(
    std::unique_ptr<HttpAuthHandlerFactory> factory) {
  http_auth_handler_factory_ = std::move(factory);
}
void URLRequestContext::set_http_network_session(
    std::unique_ptr<HttpNetworkSession> http_network_session) {
  http_network_session_ = std::move(http_network_session);
}
void URLRequestContext::set_http_transaction_factory(
    std::unique_ptr<HttpTransactionFactory> factory) {
  http_transaction_factory_ = std::move(factory);
}
void URLRequestContext::set_network_delegate(
    std::unique_ptr<NetworkDelegate> network_delegate) {
  network_delegate_ = std::move(network_delegate);
}
void URLRequestContext::set_http_server_properties(
    std::unique_ptr<HttpServerProperties> http_server_properties) {
  http_server_properties_ = std::move(http_server_properties);
}
void URLRequestContext::set_cookie_store(
    std::unique_ptr<CookieStore> cookie_store) {
  cookie_store_ = std::move(cookie_store);
}
void URLRequestContext::set_transport_security_state(
    std::unique_ptr<TransportSecurityState> state) {
  transport_security_state_ = std::move(state);
}
void URLRequestContext::set_ct_policy_enforcer(
    std::unique_ptr<CTPolicyEnforcer> enforcer) {
  ct_policy_enforcer_ = std::move(enforcer);
}
void URLRequestContext::set_sct_auditing_delegate(
    std::unique_ptr<SCTAuditingDelegate> delegate) {
  sct_auditing_delegate_ = std::move(delegate);
}
void URLRequestContext::set_job_factory(
    std::unique_ptr<const URLRequestJobFactory> job_factory) {
  job_factory_storage_ = std::move(job_factory);
  job_factory_ = job_factory_storage_.get();
}
void URLRequestContext::set_throttler_manager(
    std::unique_ptr<URLRequestThrottlerManager> throttler_manager) {
  throttler_manager_ = std::move(throttler_manager);
}
void URLRequestContext::set_quic_context(
    std::unique_ptr<QuicContext> quic_context) {
  quic_context_ = std::move(quic_context);
}
void URLRequestContext::set_http_user_agent_settings(
    std::unique_ptr<const HttpUserAgentSettings> http_user_agent_settings) {
  http_user_agent_settings_ = std::move(http_user_agent_settings);
}
void URLRequestContext::set_network_quality_estimator(
    NetworkQualityEstimator* network_quality_estimator) {
  network_quality_estimator_ = network_quality_estimator;
}
void URLRequestContext::set_client_socket_factory(
    std::unique_ptr<ClientSocketFactory> client_socket_factory) {
  client_socket_factory_ = std::move(client_socket_factory);
}
#if BUILDFLAG(ENABLE_REPORTING)
void URLRequestContext::set_persistent_reporting_and_nel_store(
    std::unique_ptr<PersistentReportingAndNelStore>
        persistent_reporting_and_nel_store) {
  persistent_reporting_and_nel_store_ =
      std::move(persistent_reporting_and_nel_store);
}
void URLRequestContext::set_reporting_service(
    std::unique_ptr<ReportingService> reporting_service) {
  reporting_service_ = std::move(reporting_service);
}
void URLRequestContext::set_network_error_logging_service(
    std::unique_ptr<NetworkErrorLoggingService> network_error_logging_service) {
  network_error_logging_service_ = std::move(network_error_logging_service);
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void URLRequestContext::set_transport_security_persister(
    std::unique_ptr<TransportSecurityPersister> transport_security_persister) {
  transport_security_persister_ = std::move(transport_security_persister);
}

}  // namespace net
