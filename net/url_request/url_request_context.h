// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's necessary when processing resource requests.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/net_buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {
class CertVerifier;
class CookieStore;
class CTPolicyEnforcer;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetLog;
class NetworkDelegate;
class NetworkQualityEstimator;
class ProxyDelegate;
class ProxyResolutionService;
class QuicContext;
class SCTAuditingDelegate;
class SSLConfigService;
class URLRequest;
class URLRequestJobFactory;
class URLRequestThrottlerManager;

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
class FtpAuthCache;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if BUILDFLAG(ENABLE_REPORTING)
class NetworkErrorLoggingService;
class ReportingService;
#endif  // BUILDFLAG(ENABLE_REPORTING)

// Subclass to provide application-specific context for URLRequest
// instances. URLRequestContext does not own these member variables, since they
// may be shared with other contexts. URLRequestContextStorage can be used for
// automatic lifetime management. Most callers should use an existing
// URLRequestContext rather than creating a new one, as guaranteeing that the
// URLRequestContext is destroyed before its members can be difficult.
class NET_EXPORT URLRequestContext
    : public base::trace_event::MemoryDumpProvider {
 public:
  URLRequestContext();
  ~URLRequestContext() override;

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSession::Params* GetNetworkSessionParams() const;

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSession::Context* GetNetworkSessionContext() const;

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if !defined(OS_WIN) && !(defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // This function should not be used in Chromium, please use the version with
  // NetworkTrafficAnnotationTag in the future.
  //
  // The unannotated method is not available on desktop Linux + Windows. It's
  // available on other platforms, since we only audit network annotations on
  // Linux & Windows.
  std::unique_ptr<URLRequest> CreateRequest(
      const GURL& url,
      RequestPriority priority,
      URLRequest::Delegate* delegate) const;
#endif

  // |traffic_annotation| is metadata about the network traffic send via this
  // URLRequest, see net::DefineNetworkTrafficAnnotation. Note that:
  // - net provides the API for tagging requests with an opaque identifier.
  // - chrome/browser/privacy/traffic_annotation.proto contains the Chrome
  // specific .proto describing the verbose annotation format that Chrome's
  // callsites are expected to follow.
  // - tools/traffic_annotation/ contains sample and template for annotation and
  // tools will be added for verification following crbug.com/690323.
  std::unique_ptr<URLRequest> CreateRequest(
      const GURL& url,
      RequestPriority priority,
      URLRequest::Delegate* delegate,
      NetworkTrafficAnnotationTag traffic_annotation) const;

  NetLog* net_log() const { return net_log_; }

  void set_net_log(NetLog* net_log) {
    net_log_ = net_log;
  }

  HostResolver* host_resolver() const {
    return host_resolver_;
  }

  void set_host_resolver(HostResolver* host_resolver) {
    DCHECK(host_resolver);
    host_resolver_ = host_resolver;
  }

  CertVerifier* cert_verifier() const {
    return cert_verifier_;
  }

  void set_cert_verifier(CertVerifier* cert_verifier) {
    cert_verifier_ = cert_verifier;
  }

  // Get the proxy service for this context.
  ProxyResolutionService* proxy_resolution_service() const {
    return proxy_resolution_service_;
  }
  void set_proxy_resolution_service(
      ProxyResolutionService* proxy_resolution_service) {
    proxy_resolution_service_ = proxy_resolution_service;
  }

  ProxyDelegate* proxy_delegate() const { return proxy_delegate_; }
  void set_proxy_delegate(ProxyDelegate* proxy_delegate) {
    proxy_delegate_ = proxy_delegate;
  }

  // Get the ssl config service for this context.
  SSLConfigService* ssl_config_service() const { return ssl_config_service_; }
  void set_ssl_config_service(SSLConfigService* service) {
    ssl_config_service_ = service;
  }

  // Gets the HTTP Authentication Handler Factory for this context.
  // The factory is only valid for the lifetime of this URLRequestContext
  HttpAuthHandlerFactory* http_auth_handler_factory() const {
    return http_auth_handler_factory_;
  }
  void set_http_auth_handler_factory(HttpAuthHandlerFactory* factory) {
    http_auth_handler_factory_ = factory;
  }

  // Gets the http transaction factory for this context.
  HttpTransactionFactory* http_transaction_factory() const {
    return http_transaction_factory_;
  }
  void set_http_transaction_factory(HttpTransactionFactory* factory) {
    http_transaction_factory_ = factory;
  }

  void set_network_delegate(NetworkDelegate* network_delegate) {
    network_delegate_ = network_delegate;
  }
  NetworkDelegate* network_delegate() const { return network_delegate_; }

  void set_http_server_properties(
      HttpServerProperties* http_server_properties) {
    http_server_properties_ = http_server_properties;
  }
  HttpServerProperties* http_server_properties() const {
    return http_server_properties_;
  }

  // Gets the cookie store for this context (may be null, in which case
  // cookies are not stored).
  CookieStore* cookie_store() const { return cookie_store_; }
  void set_cookie_store(CookieStore* cookie_store);

  TransportSecurityState* transport_security_state() const {
    return transport_security_state_;
  }
  void set_transport_security_state(
      TransportSecurityState* state) {
    transport_security_state_ = state;
  }

  CTPolicyEnforcer* ct_policy_enforcer() const { return ct_policy_enforcer_; }
  void set_ct_policy_enforcer(CTPolicyEnforcer* enforcer) {
    ct_policy_enforcer_ = enforcer;
  }

  SCTAuditingDelegate* sct_auditing_delegate() const {
    return sct_auditing_delegate_;
  }
  void set_sct_auditing_delegate(SCTAuditingDelegate* delegate) {
    sct_auditing_delegate_ = delegate;
  }

  const URLRequestJobFactory* job_factory() const { return job_factory_; }
  void set_job_factory(const URLRequestJobFactory* job_factory) {
    job_factory_ = job_factory;
  }

  // May return nullptr.
  URLRequestThrottlerManager* throttler_manager() const {
    return throttler_manager_;
  }
  void set_throttler_manager(URLRequestThrottlerManager* throttler_manager) {
    throttler_manager_ = throttler_manager;
  }

  QuicContext* quic_context() const { return quic_context_; }
  void set_quic_context(QuicContext* quic_context) {
    quic_context_ = quic_context;
  }

  // Gets the URLRequest objects that hold a reference to this
  // URLRequestContext.
  std::set<const URLRequest*>* url_requests() const {
    return url_requests_.get();
  }

  // CHECKs that no URLRequests using this context remain. Subclasses should
  // additionally call AssertNoURLRequests() within their own destructor,
  // prior to implicit destruction of subclass-owned state.
  void AssertNoURLRequests() const;

  // Get the underlying |HttpUserAgentSettings| implementation that provides
  // the HTTP Accept-Language and User-Agent header values.
  const HttpUserAgentSettings* http_user_agent_settings() const {
    return http_user_agent_settings_;
  }
  void set_http_user_agent_settings(
      const HttpUserAgentSettings* http_user_agent_settings) {
    http_user_agent_settings_ = http_user_agent_settings;
  }

  // Gets the NetworkQualityEstimator associated with this context.
  // May return nullptr.
  NetworkQualityEstimator* network_quality_estimator() const {
    return network_quality_estimator_;
  }
  void set_network_quality_estimator(
      NetworkQualityEstimator* network_quality_estimator) {
    network_quality_estimator_ = network_quality_estimator;
  }

#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* reporting_service() const { return reporting_service_; }
  void set_reporting_service(ReportingService* reporting_service) {
    reporting_service_ = reporting_service;
  }

  NetworkErrorLoggingService* network_error_logging_service() const {
    return network_error_logging_service_;
  }
  void set_network_error_logging_service(
      NetworkErrorLoggingService* network_error_logging_service) {
    network_error_logging_service_ = network_error_logging_service;
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  void set_enable_brotli(bool enable_brotli) { enable_brotli_ = enable_brotli; }

  bool enable_brotli() const { return enable_brotli_; }

  // Sets the |check_cleartext_permitted| flag, which controls whether to check
  // system policy before allowing a cleartext http or ws request.
  void set_check_cleartext_permitted(bool check_cleartext_permitted) {
    check_cleartext_permitted_ = check_cleartext_permitted;
  }

  // Returns current value of the |check_cleartext_permitted| flag.
  bool check_cleartext_permitted() const { return check_cleartext_permitted_; }

  void set_require_network_isolation_key(bool require_network_isolation_key) {
    require_network_isolation_key_ = require_network_isolation_key;
  }
  bool require_network_isolation_key() const {
    return require_network_isolation_key_;
  }

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  void set_ftp_auth_cache(FtpAuthCache* auth_cache) {
    ftp_auth_cache_ = auth_cache;
  }
  FtpAuthCache* ftp_auth_cache() { return ftp_auth_cache_; }
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  // Sets a name for this URLRequestContext. Currently the name is used in
  // MemoryDumpProvier to annotate memory usage. The name does not need to be
  // unique.
  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  // MemoryDumpProvider implementation:
  // This is reported as
  // "memory:chrome:all_processes:reported_by_chrome:net:effective_size_avg."
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void AssertCalledOnValidThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

 private:
  // Ownership for these members are not defined here. Clients should either
  // provide storage elsewhere or have a subclass take ownership.
  NetLog* net_log_;
  HostResolver* host_resolver_;
  CertVerifier* cert_verifier_;
  HttpAuthHandlerFactory* http_auth_handler_factory_;
  ProxyResolutionService* proxy_resolution_service_;
  ProxyDelegate* proxy_delegate_;
  SSLConfigService* ssl_config_service_;
  NetworkDelegate* network_delegate_;
  HttpServerProperties* http_server_properties_;
  const HttpUserAgentSettings* http_user_agent_settings_;
  CookieStore* cookie_store_;
  TransportSecurityState* transport_security_state_;
  CTPolicyEnforcer* ct_policy_enforcer_;
  SCTAuditingDelegate* sct_auditing_delegate_;
  HttpTransactionFactory* http_transaction_factory_;
  const URLRequestJobFactory* job_factory_;
  URLRequestThrottlerManager* throttler_manager_;
  QuicContext* quic_context_;
  NetworkQualityEstimator* network_quality_estimator_;
#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* reporting_service_;
  NetworkErrorLoggingService* network_error_logging_service_;
#endif  // BUILDFLAG(ENABLE_REPORTING)
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  FtpAuthCache* ftp_auth_cache_;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  std::unique_ptr<std::set<const URLRequest*>> url_requests_;

  // Enables Brotli Content-Encoding support.
  bool enable_brotli_;
  // Enables checking system policy before allowing a cleartext http or ws
  // request. Only used on Android.
  bool check_cleartext_permitted_;

  // Triggers a DCHECK if a NetworkIsolationKey/IsolationInfo is not provided to
  // a request when true.
  bool require_network_isolation_key_;

  // An optional name which can be set to describe this URLRequestContext.
  // Used in MemoryDumpProvier to annotate memory usage. The name does not need
  // to be unique.
  std::string name_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
