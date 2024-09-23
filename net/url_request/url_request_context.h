// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's necessary when processing resource requests.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_source.h"
#include "net/net_buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

namespace net {
class CertVerifier;
class ClientSocketFactory;
class CookieStore;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkSession;
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
class TransportSecurityPersister;
class TransportSecurityState;
class URLRequest;
class URLRequestJobFactory;
class URLRequestContextBuilder;

#if BUILDFLAG(ENABLE_REPORTING)
class NetworkErrorLoggingService;
class PersistentReportingAndNelStore;
class ReportingService;
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
namespace device_bound_sessions {
class SessionService;
}
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

// Class that provides application-specific context for URLRequest
// instances. May only be created by URLRequestContextBuilder.
// Owns most of its member variables, except a few that may be shared
// with other contexts.
class NET_EXPORT URLRequestContext final {
 public:
  // URLRequestContext must be created by URLRequestContextBuilder.
  explicit URLRequestContext(base::PassKey<URLRequestContextBuilder> pass_key);
  URLRequestContext(const URLRequestContext&) = delete;
  URLRequestContext& operator=(const URLRequestContext&) = delete;

  ~URLRequestContext();

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSessionParams* GetNetworkSessionParams() const;

  // May return nullptr if this context doesn't have an associated network
  // session.
  const HttpNetworkSessionContext* GetNetworkSessionContext() const;

// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
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
      const std::optional<net::NetLogSource> net_log_source =
          std::nullopt) const;

  NetLog* net_log() const { return net_log_; }

  HostResolver* host_resolver() const { return host_resolver_.get(); }

  CertVerifier* cert_verifier() const { return cert_verifier_.get(); }

  // Get the proxy service for this context.
  ProxyResolutionService* proxy_resolution_service() const {
    return proxy_resolution_service_.get();
  }

  ProxyDelegate* proxy_delegate() const { return proxy_delegate_.get(); }

  // Get the ssl config service for this context.
  SSLConfigService* ssl_config_service() const {
    return ssl_config_service_.get();
  }

  // Gets the HTTP Authentication Handler Factory for this context.
  // The factory is only valid for the lifetime of this URLRequestContext
  HttpAuthHandlerFactory* http_auth_handler_factory() const {
    return http_auth_handler_factory_.get();
  }

  // Gets the http transaction factory for this context.
  HttpTransactionFactory* http_transaction_factory() const {
    return http_transaction_factory_.get();
  }

  NetworkDelegate* network_delegate() const { return network_delegate_.get(); }

  HttpServerProperties* http_server_properties() const {
    return http_server_properties_.get();
  }

  // Gets the cookie store for this context (may be null, in which case
  // cookies are not stored).
  CookieStore* cookie_store() const { return cookie_store_.get(); }

  TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

  SCTAuditingDelegate* sct_auditing_delegate() const {
    return sct_auditing_delegate_.get();
  }

  const URLRequestJobFactory* job_factory() const { return job_factory_; }

  QuicContext* quic_context() const { return quic_context_.get(); }

  // Gets the URLRequest objects that hold a reference to this
  // URLRequestContext.
  std::set<raw_ptr<const URLRequest, SetExperimental>>* url_requests() const {
    return url_requests_.get();
  }

  // CHECKs that no URLRequests using this context remain. Subclasses should
  // additionally call AssertNoURLRequests() within their own destructor,
  // prior to implicit destruction of subclass-owned state.
  void AssertNoURLRequests() const;

  // Get the underlying |HttpUserAgentSettings| implementation that provides
  // the HTTP Accept-Language and User-Agent header values.
  const HttpUserAgentSettings* http_user_agent_settings() const {
    return http_user_agent_settings_.get();
  }

  // Gets the NetworkQualityEstimator associated with this context.
  // May return nullptr.
  NetworkQualityEstimator* network_quality_estimator() const {
    return network_quality_estimator_.get();
  }

#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* reporting_service() const {
    return reporting_service_.get();
  }

  NetworkErrorLoggingService* network_error_logging_service() const {
    return network_error_logging_service_.get();
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  // May return nullptr if the feature is disabled.
  device_bound_sessions::SessionService* device_bound_session_service() const {
    return device_bound_session_service_.get();
  }
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

  bool enable_brotli() const { return enable_brotli_; }

  bool enable_zstd() const { return enable_zstd_; }

  // Returns current value of the |check_cleartext_permitted| flag.
  bool check_cleartext_permitted() const { return check_cleartext_permitted_; }

  bool require_network_anonymization_key() const {
    return require_network_anonymization_key_;
  }

  // If != handles::kInvalidNetworkHandle, the network which this
  // context has been bound to.
  handles::NetworkHandle bound_network() const { return bound_network_; }

  void AssertCalledOnValidThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  // DEPRECATED: Do not use this even in tests. This is for a legacy use.
  void SetJobFactoryForTesting(const URLRequestJobFactory* job_factory) {
    job_factory_ = job_factory;
  }

  const std::optional<std::string>& cookie_deprecation_label() const {
    return cookie_deprecation_label_;
  }

  void set_cookie_deprecation_label(const std::optional<std::string>& label) {
    cookie_deprecation_label_ = label;
  }

 private:
  friend class URLRequestContextBuilder;

  HttpNetworkSession* http_network_session() const {
    return http_network_session_.get();
  }

  void set_net_log(NetLog* net_log);
  void set_host_resolver(std::unique_ptr<HostResolver> host_resolver);
  void set_cert_verifier(std::unique_ptr<CertVerifier> cert_verifier);
  void set_proxy_resolution_service(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service);
  void set_proxy_delegate(std::unique_ptr<ProxyDelegate> proxy_delegate);
  void set_ssl_config_service(std::unique_ptr<SSLConfigService> service);
  void set_http_auth_handler_factory(
      std::unique_ptr<HttpAuthHandlerFactory> factory);
  void set_http_network_session(
      std::unique_ptr<HttpNetworkSession> http_network_session);
  void set_http_transaction_factory(
      std::unique_ptr<HttpTransactionFactory> factory);
  void set_network_delegate(std::unique_ptr<NetworkDelegate> network_delegate);
  void set_http_server_properties(
      std::unique_ptr<HttpServerProperties> http_server_properties);
  void set_cookie_store(std::unique_ptr<CookieStore> cookie_store);
  void set_transport_security_state(
      std::unique_ptr<TransportSecurityState> state);
  void set_sct_auditing_delegate(std::unique_ptr<SCTAuditingDelegate> delegate);
  void set_job_factory(std::unique_ptr<const URLRequestJobFactory> job_factory);
  void set_quic_context(std::unique_ptr<QuicContext> quic_context);
  void set_http_user_agent_settings(
      std::unique_ptr<const HttpUserAgentSettings> http_user_agent_settings);
  void set_network_quality_estimator(
      NetworkQualityEstimator* network_quality_estimator);
  void set_client_socket_factory(
      std::unique_ptr<ClientSocketFactory> client_socket_factory);
#if BUILDFLAG(ENABLE_REPORTING)
  void set_persistent_reporting_and_nel_store(
      std::unique_ptr<PersistentReportingAndNelStore>
          persistent_reporting_and_nel_store);
  void set_reporting_service(
      std::unique_ptr<ReportingService> reporting_service);
  void set_network_error_logging_service(
      std::unique_ptr<NetworkErrorLoggingService>
          network_error_logging_service);
#endif  // BUILDFLAG(ENABLE_REPORTING)
  void set_enable_brotli(bool enable_brotli) { enable_brotli_ = enable_brotli; }
  void set_enable_zstd(bool enable_zstd) { enable_zstd_ = enable_zstd; }
  void set_check_cleartext_permitted(bool check_cleartext_permitted) {
    check_cleartext_permitted_ = check_cleartext_permitted;
  }
  void set_require_network_anonymization_key(
      bool require_network_anonymization_key) {
    require_network_anonymization_key_ = require_network_anonymization_key;
  }
  void set_bound_network(handles::NetworkHandle network) {
    bound_network_ = network;
  }

  void set_transport_security_persister(
      std::unique_ptr<TransportSecurityPersister> transport_security_persister);

  raw_ptr<NetLog> net_log_ = nullptr;
#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  void set_device_bound_session_service(
      std::unique_ptr<device_bound_sessions::SessionService>
          device_bound_session_service);
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

  std::unique_ptr<HostResolver> host_resolver_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  std::unique_ptr<NetworkDelegate> network_delegate_;
  // `proxy_resolution_service_` may store a pointer to `proxy_delegate_`, so
  // ensure that the latter outlives the former.
  std::unique_ptr<ProxyDelegate> proxy_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  std::unique_ptr<const HttpUserAgentSettings> http_user_agent_settings_;
  std::unique_ptr<CookieStore> cookie_store_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  std::unique_ptr<SCTAuditingDelegate> sct_auditing_delegate_;
  std::unique_ptr<QuicContext> quic_context_;
  std::unique_ptr<ClientSocketFactory> client_socket_factory_;

  // The storage duplication for URLRequestJobFactory is needed because of
  // SetJobFactoryForTesting. Once this method is removable, we can only store a
  // unique_ptr similarly to the other fields.
  std::unique_ptr<const URLRequestJobFactory> job_factory_storage_;
  raw_ptr<const URLRequestJobFactory> job_factory_ = nullptr;

#if BUILDFLAG(ENABLE_REPORTING)
  // Must precede |reporting_service_| and |network_error_logging_service_|
  std::unique_ptr<PersistentReportingAndNelStore>
      persistent_reporting_and_nel_store_;

  std::unique_ptr<ReportingService> reporting_service_;
  std::unique_ptr<NetworkErrorLoggingService> network_error_logging_service_;
#endif  // BUILDFLAG(ENABLE_REPORTING)

  // May be used (but not owned) by the HttpTransactionFactory.
  std::unique_ptr<HttpNetworkSession> http_network_session_;

  // `http_transaction_factory_` might hold a raw pointer on
  // `http_network_session_` so it needs to be declared last.
  std::unique_ptr<HttpTransactionFactory> http_transaction_factory_;

  raw_ptr<NetworkQualityEstimator> network_quality_estimator_ = nullptr;

  std::unique_ptr<TransportSecurityPersister> transport_security_persister_;

  std::unique_ptr<std::set<raw_ptr<const URLRequest, SetExperimental>>>
      url_requests_;

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  std::unique_ptr<device_bound_sessions::SessionService>
      device_bound_session_service_;
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

  // Enables Brotli Content-Encoding support.
  bool enable_brotli_ = false;
  // Enables Zstd Content-Encoding support.
  bool enable_zstd_ = false;
  // Enables checking system policy before allowing a cleartext http or ws
  // request. Only used on Android.
  bool check_cleartext_permitted_ = false;

  // Triggers a DCHECK if a NetworkAnonymizationKey/IsolationInfo is not
  // provided to a request when true.
  bool require_network_anonymization_key_ = false;

  std::optional<std::string> cookie_deprecation_label_;

  handles::NetworkHandle bound_network_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
