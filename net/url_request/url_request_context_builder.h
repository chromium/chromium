// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is useful for building a simple URLRequestContext. Most creators
// of new URLRequestContexts should use this helper class to construct it. Call
// any configuration params, and when done, invoke Build() to construct the
// URLRequestContext. This URLRequestContext will own all its own storage.
//
// URLRequestContextBuilder and its associated params classes are initially
// populated with "sane" default values. Read through the comments to figure out
// what these are.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_delegate.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/ssl_config_service.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
namespace android {
class ApplicationStatusListener;
}
}  // namespace base

namespace net {

class CertVerifier;
class ChannelIDService;
class CookieStore;
class CTPolicyEnforcer;
class CTVerifier;
class HttpAuthHandlerFactory;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class HttpServerProperties;
class NetworkQualityEstimator;
class ProxyConfigService;
class URLRequestContext;
class URLRequestInterceptor;

#if BUILDFLAG(ENABLE_REPORTING)
struct ReportingPolicy;
#endif  // BUILDFLAG(ENABLE_REPORTING)

// A URLRequestContextBuilder creates a single URLRequestContext. It provides
// methods to manage various URLRequestContext components which should be called
// before creating the Context. Once configuration is complete, calling Build()
// will create a URLRequestContext with the specified configuration. Components
// that are not explicitly configured will use reasonable in-memory defaults.
//
// The returned URLRequestContext is self-contained: Deleting it will safely
// shut down all of the URLRequests owned by its internal components, and then
// tear down those components. The only exception to this are objects not owned
// by URLRequestContext. This includes components passed in to the methods that
// take raw pointers, and objects that components passed in to the Builder have
// raw pointers to.
//
// A Builder should be destroyed after calling Build, and there is no need to
// keep it around for the lifetime of the created URLRequestContext. Each
// Builder may be used to create only a single URLRequestContext.
class NET_EXPORT URLRequestContextBuilder {
 public:
  // Creates a LayeredDelegate that wraps |inner_network_delegate|.
  using CreateLayeredNetworkDelegate =
      base::OnceCallback<std::unique_ptr<NetworkDelegate>(
          std::unique_ptr<NetworkDelegate> inner_network_delegate)>;

  using CreateInterceptingJobFactory =
      base::OnceCallback<std::unique_ptr<URLRequestJobFactory>(
          std::unique_ptr<URLRequestJobFactory> inner_job_factory)>;

  // Creates an HttpNetworkTransactionFactory given an HttpNetworkSession. Does
  // not take ownership of the session.
  using CreateHttpTransactionFactoryCallback =
      base::OnceCallback<std::unique_ptr<HttpTransactionFactory>(
          HttpNetworkSession* session)>;

  struct NET_EXPORT HttpCacheParams {
    enum Type {
      // In-memory cache.
      IN_MEMORY,
      // Disk cache using "default" backend.
      DISK,
      // Disk cache using "blockfile" backend (BackendImpl).
      DISK_BLOCKFILE,
      // Disk cache using "simple" backend (SimpleBackendImpl).
      DISK_SIMPLE,
    };

    HttpCacheParams();
    ~HttpCacheParams();

    // The type of HTTP cache. Default is IN_MEMORY.
    Type type;

    // The max size of the cache in bytes. Default is algorithmically determined
    // based off available disk space.
    int max_size;

    // The cache path (when type is DISK).
    base::FilePath path;

#if defined(OS_ANDROID)
    // If this is set, will override the default ApplicationStatusListener. This
    // is useful if the cache will not be in the main process.
    base::android::ApplicationStatusListener* app_status_listener = nullptr;
#endif
  };

  URLRequestContextBuilder();
  virtual ~URLRequestContextBuilder();

  // Sets a name for this URLRequestContext. Currently the name is used in
  // MemoryDumpProvier to annotate memory usage. The name does not need to be
  // unique.
  void set_name(const std::string& name) { name_ = name; }

  // Sets whether Brotli compression is enabled.  Disabled by default;
  void set_enable_brotli(bool enable_brotli) { enable_brotli_ = enable_brotli; }

  // Unlike most other setters, the builder does not take ownership of the
  // NetworkQualityEstimator.
  void set_network_quality_estimator(
      NetworkQualityEstimator* network_quality_estimator) {
    network_quality_estimator_ = network_quality_estimator;
  }

  // Extracts the component pointers required to construct an HttpNetworkSession
  // and copies them into the HttpNetworkSession::Context used to create the
  // session. This function should be used to ensure that a context and its
  // associated HttpNetworkSession are consistent.
  static void SetHttpNetworkSessionComponents(
      const URLRequestContext* request_context,
      HttpNetworkSession::Context* session_context);

  // These functions are mutually exclusive.  The ProxyConfigService, if
  // set, will be used to construct a ProxyResolutionService.
  void set_proxy_config_service(
      std::unique_ptr<ProxyConfigService> proxy_config_service) {
    proxy_config_service_ = std::move(proxy_config_service);
  }

  // Sets whether quick PAC checks are enabled. Defaults to true. Ignored if
  // a ProxyResolutionService is set directly.
  void set_pac_quick_check_enabled(bool pac_quick_check_enabled) {
    pac_quick_check_enabled_ = pac_quick_check_enabled;
  }

  // Sets policy for sanitizing URLs before passing them to a PAC. Defaults to
  // ProxyResolutionService::SanitizeUrlPolicy::SAFE. Ignored if
  // a ProxyResolutionService is set directly.
  void set_pac_sanitize_url_policy(
      ProxyResolutionService::SanitizeUrlPolicy pac_sanitize_url_policy) {
    pac_sanitize_url_policy_ = pac_sanitize_url_policy;
  }

  // Sets the proxy service. If one is not provided, by default, uses system
  // libraries to evaluate PAC scripts, if available (And if not, skips PAC
  // resolution). Subclasses may override CreateProxyResolutionService for
  // different default behavior.
  void set_proxy_resolution_service(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    proxy_resolution_service_ = std::move(proxy_resolution_service);
  }

  void set_ssl_config_service(
      std::unique_ptr<SSLConfigService> ssl_config_service) {
    ssl_config_service_ = std::move(ssl_config_service);
  }

  // Call these functions to specify hard-coded Accept-Language
  // or User-Agent header values for all requests that don't
  // have the headers already set.
  void set_accept_language(const std::string& accept_language);
  void set_user_agent(const std::string& user_agent);

  // Makes the created URLRequestContext use a particular HttpUserAgentSettings
  // object. Not compatible with set_accept_language() / set_user_agent().
  //
  // The object will be live until the URLRequestContext is destroyed.
  void set_http_user_agent_settings(
      std::unique_ptr<HttpUserAgentSettings> http_user_agent_settings);

  // Control support for data:// requests. By default it's disabled.
  void set_data_enabled(bool enable) {
    data_enabled_ = enable;
  }

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  // Control support for file:// requests. By default it's disabled.
  void set_file_enabled(bool enable) {
    file_enabled_ = enable;
  }
#endif

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  // Control support for ftp:// requests. By default it's disabled.
  void set_ftp_enabled(bool enable) {
    ftp_enabled_ = enable;
  }
#endif

  // Sets a valid ProtocolHandler for a scheme.
  // A ProtocolHandler already exists for |scheme| will be overwritten.
  void SetProtocolHandler(
      const std::string& scheme,
      std::unique_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler);

  // Unlike the other setters, the builder does not take ownership of the
  // NetLog.
  // TODO(mmenke):  Probably makes sense to get rid of this, and have consumers
  // set their own NetLog::Observers instead.
  void set_net_log(NetLog* net_log) { net_log_ = net_log; }

  // By default host_resolver is constructed with CreateDefaultResolver.
  void set_host_resolver(std::unique_ptr<HostResolver> host_resolver);
  // Allows sharing the HostResolver with other URLRequestContexts. Should not
  // be used if set_host_resolver() is used. The consumer must ensure the
  // HostResolver outlives the URLRequestContext returned by the builder.
  //
  // TODO(mmenke): Figure out the cost/benefits of not supporting sharing
  // HostResolvers between URLRequestContexts. See: https://crbug.com/743251.
  void set_shared_host_resolver(HostResolver* host_resolver);

  // Uses BasicNetworkDelegate by default. Note that calling Build will unset
  // any custom delegate in builder, so this must be called each time before
  // Build is called.
  void set_network_delegate(std::unique_ptr<NetworkDelegate> delegate) {
    network_delegate_ = std::move(delegate);
  }

  // Sets an optional callback that creates a NetworkDelegate wrapping either
  // the default NetworkDelegate, or the one set by the above method.
  // TODO(mmenke): Remove this once the network service ships.
  void SetCreateLayeredNetworkDelegateCallback(
      CreateLayeredNetworkDelegate create_layered_network_delegate_callback);

  // Sets the ProxyDelegate.
  void set_proxy_delegate(std::unique_ptr<ProxyDelegate> proxy_delegate);
  // Allows sharing the PreoxyDelegates with other URLRequestContexts. Should
  // not be used if set_proxy_delegate() is used. The consumer must ensure the
  // ProxyDelegate outlives the URLRequestContext returned by the builder.
  //
  // TODO(mmenke): Remove this (And update consumers). See:
  // https://crbug.com/743251.
  void set_shared_proxy_delegate(ProxyDelegate* shared_proxy_delegate);

  // Sets a specific HttpAuthHandlerFactory to be used by the URLRequestContext
  // rather than the default |HttpAuthHandlerRegistryFactory|. The builder
  // takes ownership of the factory and will eventually transfer it to the new
  // URLRequestContext.
  void SetHttpAuthHandlerFactory(
      std::unique_ptr<HttpAuthHandlerFactory> factory);
  // Makes the created URLRequestContext use a shared HttpAuthHandlerFactory
  // object. Not compatible with SetHttpAuthHandlerFactory(). The consumer must
  // ensure the HttpAuthHandlerFactory outlives the URLRequestContext returned
  // by the builder.
  //
  // TODO(mmenke): Evaluate if sharing is really needed. See:
  // https://crbug.com/743251.
  void set_shared_http_auth_handler_factory(
      HttpAuthHandlerFactory* shared_http_auth_handler_factory);

  // By default HttpCache is enabled with a default constructed HttpCacheParams.
  void EnableHttpCache(const HttpCacheParams& params);
  void DisableHttpCache();

  // Override default HttpNetworkSession::Params settings.
  void set_http_network_session_params(
      const HttpNetworkSession::Params& http_network_session_params) {
    http_network_session_params_ = http_network_session_params;
  }

  void set_transport_security_persister_path(
      const base::FilePath& transport_security_persister_path) {
    transport_security_persister_path_ = transport_security_persister_path;
  }

  void SetSpdyAndQuicEnabled(bool spdy_enabled,
                             bool quic_enabled);

  void set_throttling_enabled(bool throttling_enabled) {
    throttling_enabled_ = throttling_enabled;
  }

  void set_ct_verifier(std::unique_ptr<CTVerifier> ct_verifier);
  void set_ct_policy_enforcer(
      std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer);

  void SetCertVerifier(std::unique_ptr<CertVerifier> cert_verifier);
  // Same as above, but does not take ownership. The CertVerifier must outlive
  // the created URLRequestContext.
  // TODO(mmenke): Remove once no longer needed.
  void SetSharedCertVerifier(CertVerifier* shared_cert_verifier);

#if BUILDFLAG(ENABLE_REPORTING)
  void set_reporting_policy(std::unique_ptr<ReportingPolicy> reporting_policy);

  void set_network_error_logging_enabled(bool network_error_logging_enabled) {
    network_error_logging_enabled_ = network_error_logging_enabled;
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  void SetInterceptors(std::vector<std::unique_ptr<URLRequestInterceptor>>
                           url_request_interceptors);

  // Sets a callback that is passed ownership of the URLRequestJobFactory, and
  // can wrap it in another URLRequestJobFactory. URLRequestInterceptors can't
  // handle intercepting unsupported protocols, while this case.
  // TODO(mmenke): Remove this, once it's no longer needed.
  void set_create_intercepting_job_factory(
      CreateInterceptingJobFactory create_intercepting_job_factory);

  // Override the default in-memory cookie store and channel id service.
  // If both |cookie_store| and |channel_id_service| are NULL, CookieStore and
  // ChannelIDService will be disabled for this context.
  // If |cookie_store| is not NULL and |channel_id_service| is NULL,
  // only ChannelIdService is disabled for this context.
  // Note that a persistent cookie store should not be used with an in-memory
  // channel id service, and one cookie store should not be shared between
  // multiple channel-id stores (or used both with and without a channel id
  // store).
  void SetCookieAndChannelIdStores(
      std::unique_ptr<CookieStore> cookie_store,
      std::unique_ptr<ChannelIDService> channel_id_service);

  // Sets a specific HttpServerProperties for use in the
  // URLRequestContext rather than creating a default HttpServerPropertiesImpl.
  void SetHttpServerProperties(
      std::unique_ptr<HttpServerProperties> http_server_properties);

  // Sets a callback that will be used to create the
  // HttpNetworkTransactionFactory. If a cache is enabled, the cache's
  // HttpTransactionFactory will wrap the one this creates.
  // TODO(mmenke): Get rid of this. See https://crbug.com/721408
  void SetCreateHttpTransactionFactoryCallback(
      CreateHttpTransactionFactoryCallback
          create_http_network_transaction_factory);

  // Creates a mostly self-contained URLRequestContext. May only be called once
  // per URLRequestContextBuilder. After this is called, the Builder can be
  // safely destroyed.
  std::unique_ptr<URLRequestContext> Build();

 protected:
  // Lets subclasses override ProxyResolutionService creation, using a
  // ProxyResolutionService that uses the URLRequestContext itself to get PAC
  // scripts. When this method is invoked, the URLRequestContext is not yet
  // ready to service requests.
  virtual std::unique_ptr<ProxyResolutionService> CreateProxyResolutionService(
      std::unique_ptr<ProxyConfigService> proxy_config_service,
      URLRequestContext* url_request_context,
      HostResolver* host_resolver,
      NetworkDelegate* network_delegate,
      NetLog* net_log);

 private:
  std::string name_;
  bool enable_brotli_;
  NetworkQualityEstimator* network_quality_estimator_;

  std::string accept_language_;
  std::string user_agent_;
  std::unique_ptr<HttpUserAgentSettings> http_user_agent_settings_;

  // Include support for data:// requests.
  bool data_enabled_;
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  // Include support for file:// requests.
  bool file_enabled_;
#endif
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  // Include support for ftp:// requests.
  bool ftp_enabled_;
#endif
  bool http_cache_enabled_;
  bool throttling_enabled_;
  bool cookie_store_set_by_client_;

  HttpCacheParams http_cache_params_;
  HttpNetworkSession::Params http_network_session_params_;
  CreateHttpTransactionFactoryCallback create_http_network_transaction_factory_;
  base::FilePath transport_security_persister_path_;
  NetLog* net_log_;
  std::unique_ptr<HostResolver> host_resolver_;
  HostResolver* shared_host_resolver_;
  std::unique_ptr<ChannelIDService> channel_id_service_;
  std::unique_ptr<ProxyConfigService> proxy_config_service_;
  bool pac_quick_check_enabled_;
  ProxyResolutionService::SanitizeUrlPolicy pac_sanitize_url_policy_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  std::unique_ptr<NetworkDelegate> network_delegate_;
  CreateLayeredNetworkDelegate create_layered_network_delegate_callback_;
  std::unique_ptr<ProxyDelegate> proxy_delegate_;
  ProxyDelegate* shared_proxy_delegate_;
  std::unique_ptr<CookieStore> cookie_store_;
  std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  HttpAuthHandlerFactory* shared_http_auth_handler_factory_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  CertVerifier* shared_cert_verifier_;
  std::unique_ptr<CTVerifier> ct_verifier_;
  std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer_;
#if BUILDFLAG(ENABLE_REPORTING)
  std::unique_ptr<ReportingPolicy> reporting_policy_;
  bool network_error_logging_enabled_;
#endif  // BUILDFLAG(ENABLE_REPORTING)
  std::vector<std::unique_ptr<URLRequestInterceptor>> url_request_interceptors_;
  CreateInterceptingJobFactory create_intercepting_job_factory_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  std::map<std::string, std::unique_ptr<URLRequestJobFactory::ProtocolHandler>>
      protocol_handlers_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilder);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
