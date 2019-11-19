// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_storage.h"

#include <utility>

#include "base/logging.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_delegate.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_throttler_manager.h"

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
#include "net/ftp/ftp_auth_cache.h"
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

URLRequestContextStorage::URLRequestContextStorage(URLRequestContext* context)
    : context_(context) {
  DCHECK(context);
}

URLRequestContextStorage::~URLRequestContextStorage() = default;

void URLRequestContextStorage::set_net_log(std::unique_ptr<NetLog> net_log) {
  context_->set_net_log(net_log.get());
  net_log_ = std::move(net_log);
}

void URLRequestContextStorage::set_host_resolver(
    std::unique_ptr<HostResolver> host_resolver) {
  context_->set_host_resolver(host_resolver.get());
  host_resolver_ = std::move(host_resolver);
}

void URLRequestContextStorage::set_cert_verifier(
    std::unique_ptr<CertVerifier> cert_verifier) {
  context_->set_cert_verifier(cert_verifier.get());
  cert_verifier_ = std::move(cert_verifier);
}

void URLRequestContextStorage::set_http_auth_handler_factory(
    std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory) {
  context_->set_http_auth_handler_factory(http_auth_handler_factory.get());
  http_auth_handler_factory_ = std::move(http_auth_handler_factory);
}

void URLRequestContextStorage::set_proxy_delegate(
    std::unique_ptr<ProxyDelegate> proxy_delegate) {
  context_->set_proxy_delegate(proxy_delegate.get());
  proxy_delegate_ = std::move(proxy_delegate);
}

void URLRequestContextStorage::set_network_delegate(
    std::unique_ptr<NetworkDelegate> network_delegate) {
  context_->set_network_delegate(network_delegate.get());
  network_delegate_ = std::move(network_delegate);
}

void URLRequestContextStorage::set_proxy_resolution_service(
    std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
  context_->set_proxy_resolution_service(proxy_resolution_service.get());
  proxy_resolution_service_ = std::move(proxy_resolution_service);
}

void URLRequestContextStorage::set_ssl_config_service(
    std::unique_ptr<SSLConfigService> ssl_config_service) {
  context_->set_ssl_config_service(ssl_config_service.get());
  ssl_config_service_ = std::move(ssl_config_service);
}

void URLRequestContextStorage::set_http_server_properties(
    std::unique_ptr<HttpServerProperties> http_server_properties) {
  context_->set_http_server_properties(http_server_properties.get());
  http_server_properties_ = std::move(http_server_properties);
}

void URLRequestContextStorage::set_cookie_store(
    std::unique_ptr<CookieStore> cookie_store) {
  context_->set_cookie_store(cookie_store.get());
  cookie_store_ = std::move(cookie_store);
}

void URLRequestContextStorage::set_transport_security_state(
    std::unique_ptr<TransportSecurityState> transport_security_state) {
  context_->set_transport_security_state(transport_security_state.get());
  transport_security_state_ = std::move(transport_security_state);
}

void URLRequestContextStorage::set_cert_transparency_verifier(
    std::unique_ptr<CTVerifier> cert_transparency_verifier) {
  context_->set_cert_transparency_verifier(cert_transparency_verifier.get());
  cert_transparency_verifier_ = std::move(cert_transparency_verifier);
}

void URLRequestContextStorage::set_ct_policy_enforcer(
    std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer) {
  context_->set_ct_policy_enforcer(ct_policy_enforcer.get());
  ct_policy_enforcer_ = std::move(ct_policy_enforcer);
}

void URLRequestContextStorage::set_http_network_session(
    std::unique_ptr<HttpNetworkSession> http_network_session) {
  http_network_session_ = std::move(http_network_session);
}

void URLRequestContextStorage::set_http_transaction_factory(
    std::unique_ptr<HttpTransactionFactory> http_transaction_factory) {
  context_->set_http_transaction_factory(http_transaction_factory.get());
  http_transaction_factory_ = std::move(http_transaction_factory);
}

void URLRequestContextStorage::set_job_factory(
    std::unique_ptr<URLRequestJobFactory> job_factory) {
  context_->set_job_factory(job_factory.get());
  job_factory_ = std::move(job_factory);
}

void URLRequestContextStorage::set_throttler_manager(
    std::unique_ptr<URLRequestThrottlerManager> throttler_manager) {
  context_->set_throttler_manager(throttler_manager.get());
  throttler_manager_ = std::move(throttler_manager);
}

void URLRequestContextStorage::set_quic_context(
    std::unique_ptr<QuicContext> quic_context) {
  context_->set_quic_context(quic_context.get());
  quic_context_ = std::move(quic_context);
}

void URLRequestContextStorage::set_http_user_agent_settings(
    std::unique_ptr<HttpUserAgentSettings> http_user_agent_settings) {
  context_->set_http_user_agent_settings(http_user_agent_settings.get());
  http_user_agent_settings_ = std::move(http_user_agent_settings);
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
void URLRequestContextStorage::set_ftp_auth_cache(
    std::unique_ptr<FtpAuthCache> ftp_auth_cache) {
  context_->set_ftp_auth_cache(ftp_auth_cache.get());
  ftp_auth_cache_ = std::move(ftp_auth_cache);
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(ENABLE_REPORTING)
void URLRequestContextStorage::set_persistent_reporting_and_nel_store(
    std::unique_ptr<PersistentReportingAndNelStore>
        persistent_reporting_and_nel_store) {
  persistent_reporting_and_nel_store_ =
      std::move(persistent_reporting_and_nel_store);
}

void URLRequestContextStorage::set_reporting_service(
    std::unique_ptr<ReportingService> reporting_service) {
  context_->set_reporting_service(reporting_service.get());
  reporting_service_ = std::move(reporting_service);
}

void URLRequestContextStorage::set_network_error_logging_service(
    std::unique_ptr<NetworkErrorLoggingService> network_error_logging_service) {
  context_->set_network_error_logging_service(
      network_error_logging_service.get());
  network_error_logging_service_ = std::move(network_error_logging_service);
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

}  // namespace net
