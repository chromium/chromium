// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's necessary when processing resource requests.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_source.h"
#include "net/net_buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class CertVerifier;
class CookieStore;
class CTPolicyEnforcer;
class HostResolver;
class HttpAuthHandlerFactory;
struct HttpNetworkSessionContext;
struct HttpNetworkSessionParams;
class HttpServerProperties;
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
class TransportSecurityState;
class URLRequest;
class URLRequestJobFactory;
class URLRequestThrottlerManager;

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
class NET_EXPORT URLRequestContext {
 public:
  URLRequestContext();

  URLRequestContext(const URLRequestContext&) = delete;
  URLRequestContext& operator=(const URLRequestContext&) = delete;

  virtual ~URLRequestContext();

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSessionParams* GetNetworkSessionParams() const;

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSessionContext* GetNetworkSessionContext() const;

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if !BUILDFLAG(IS_WIN) && \
    !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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

  // `traffic_annotation` is metadata about the network traffic send via this
  // URLRequest, see net::DefineNetworkTrafficAnnotation. Note that:
  // - net provides the API for tagging requests with an opaque identifier.
  // - chrome/browser/privacy/traffic_annotation.proto contains the Chrome
  // specific .proto describing the verbose annotation format that Chrome's
  // callsites are expected to follow.
  // - tools/traffic_annotation/ contains sample and template for annotation and
  // tools will be added for verification following crbug.com/690323.
  //
  // `is_for_websockets` should be true iff this was created for use by a
  // websocket. HTTP/HTTPS requests fail if it's true, and WS/WSS requests fail
  // if it's false. This is to protect against broken consumers.
  //
  // `net_log_source_id` is used to construct NetLogWithSource using the
  // specified Source ID. This method is expected to be used when URLRequest
  // wants to take over existing NetLogSource.
  std::unique_ptr<URLRequest> CreateRequest(
      const GURL& url,
      RequestPriority priority,
      URLRequest::Delegate* delegate,
      NetworkTrafficAnnotationTag traffic_annotation,
      bool is_for_websockets = false,
      const absl::optional<net::NetLogSource> net_log_source =
          absl::nullopt) const;

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

  // If != NetworkChangeNotifier::kInvalidNetworkHandle, the network which this
  // context has been bound to.
  NetworkChangeNotifier::NetworkHandle bound_network() const {
    return bound_network_;
  }
  void set_bound_network(NetworkChangeNotifier::NetworkHandle network) {
    bound_network_ = network;
  }

  void AssertCalledOnValidThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

 private:
  // Ownership for these members are not defined here. Clients should either
  // provide storage elsewhere or have a subclass take ownership.
  raw_ptr<NetLog> net_log_;
  raw_ptr<HostResolver> host_resolver_;
  raw_ptr<CertVerifier> cert_verifier_;
  raw_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  raw_ptr<ProxyResolutionService> proxy_resolution_service_;
  raw_ptr<ProxyDelegate> proxy_delegate_;
  raw_ptr<SSLConfigService> ssl_config_service_;
  raw_ptr<NetworkDelegate> network_delegate_;
  raw_ptr<HttpServerProperties> http_server_properties_;
  raw_ptr<const HttpUserAgentSettings> http_user_agent_settings_;
  raw_ptr<CookieStore> cookie_store_;
  raw_ptr<TransportSecurityState> transport_security_state_;
  raw_ptr<CTPolicyEnforcer> ct_policy_enforcer_;
  raw_ptr<SCTAuditingDelegate> sct_auditing_delegate_;
  raw_ptr<HttpTransactionFactory> http_transaction_factory_;
  raw_ptr<const URLRequestJobFactory> job_factory_;
  raw_ptr<URLRequestThrottlerManager> throttler_manager_;
  raw_ptr<QuicContext> quic_context_;
  raw_ptr<NetworkQualityEstimator> network_quality_estimator_;
#if BUILDFLAG(ENABLE_REPORTING)
  raw_ptr<ReportingService> reporting_service_;
  raw_ptr<NetworkErrorLoggingService> network_error_logging_service_;
#endif  // BUILDFLAG(ENABLE_REPORTING)

  std::unique_ptr<std::set<const URLRequest*>> url_requests_;

  // Enables Brotli Content-Encoding support.
  bool enable_brotli_;
  // Enables checking system policy before allowing a cleartext http or ws
  // request. Only used on Android.
  bool check_cleartext_permitted_;

  // Triggers a DCHECK if a NetworkIsolationKey/IsolationInfo is not provided to
  // a request when true.
  bool require_network_isolation_key_;

  NetworkChangeNotifier::NetworkHandle bound_network_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
