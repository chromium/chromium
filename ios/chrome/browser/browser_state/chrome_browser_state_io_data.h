// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/net_types.h"
#include "net/cert/ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"

class HostContentSettingsMap;
class IOSChromeHttpUserAgentSettings;
class IOSChromeNetworkDelegate;
class IOSChromeURLRequestContextGetter;

namespace content_settings {
class CookieSettings;
}

namespace ios {
class ChromeBrowserState;
enum class ChromeBrowserStateType;
}

namespace net {
class CookieStore;
class HttpServerProperties;
class HttpTransactionFactory;
class ProxyConfigService;
class ProxyResolutionService;
class ReportSender;
class SystemCookieStore;
class TransportSecurityPersister;
class TransportSecurityState;
class URLRequestJobFactoryImpl;
}  // namespace net

// Conceptually speaking, the ChromeBrowserStateIOData represents data that
// lives on the IO thread that is owned by a ChromeBrowserState, such as, but
// not limited to, network objects like CookieMonster, HttpTransactionFactory,
// etc.
// ChromeBrowserState owns ChromeBrowserStateIOData, but will make sure to
// delete it on the IO thread.
class ChromeBrowserStateIOData {
 public:
  typedef std::vector<scoped_refptr<IOSChromeURLRequestContextGetter>>
      IOSChromeURLRequestContextGetterVector;

  virtual ~ChromeBrowserStateIOData();

  // Utility to install additional WebUI handlers into the |job_factory|.
  // Ownership of the handlers is transferred from |protocol_handlers|
  // to the |job_factory|.
  static void InstallProtocolHandlers(
      net::URLRequestJobFactoryImpl* job_factory,
      ProtocolHandlerMap* protocol_handlers);

  // Initializes the ChromeBrowserStateIOData object and primes the
  // RequestContext generation. Must be called prior to any of the Get*()
  // methods other than GetResouceContext or GetMetricsEnabledStateOnIOThread.
  void Init(ProtocolHandlerMap* protocol_handlers) const;

  net::URLRequestContext* GetMainRequestContext() const;

  // Sets the cookie store associated with a partition path.
  // The path must exist. If there is already a cookie store, it is deleted.
  void SetCookieStoreForPartitionPath(
      std::unique_ptr<net::CookieStore> cookie_store,
      const base::FilePath& partition_path);

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that browser state.
  content_settings::CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  net::TransportSecurityState* transport_security_state() const {
    return transport_security_state_.get();
  }

  ios::ChromeBrowserStateType browser_state_type() const {
    return browser_state_type_;
  }

  bool IsOffTheRecord() const;

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this browser state resides. This is safe for use from the IO
  // thread, and should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

 protected:
  // A URLRequestContext for apps that owns its cookie store and HTTP factory,
  // to ensure they are deleted.
  class AppRequestContext : public net::URLRequestContext {
   public:
    AppRequestContext();

    void SetCookieStore(std::unique_ptr<net::CookieStore> cookie_store);
    void SetHttpNetworkSession(
        std::unique_ptr<net::HttpNetworkSession> http_network_session);
    void SetHttpTransactionFactory(
        std::unique_ptr<net::HttpTransactionFactory> http_factory);
    void SetJobFactory(std::unique_ptr<net::URLRequestJobFactory> job_factory);

    ~AppRequestContext() override;

   private:
    std::unique_ptr<net::CookieStore> cookie_store_;
    std::unique_ptr<net::HttpNetworkSession> http_network_session_;
    std::unique_ptr<net::HttpTransactionFactory> http_factory_;
    std::unique_ptr<net::URLRequestJobFactory> job_factory_;
  };

  // Created on the UI thread, read on the IO thread during
  // ChromeBrowserStateIOData lazy initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    base::FilePath path;
    IOSChromeIOThread* io_thread;
    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;

    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gsettings,
    // and needs to be on the main thread.
    std::unique_ptr<net::ProxyConfigService> proxy_config_service;

    // SystemCookieStore should be initialized from the UI thread as it depends
    // on the |browser_state|.
    std::unique_ptr<net::SystemCookieStore> system_cookie_store;

    // The browser state this struct was populated from. It's passed as a void*
    // to ensure it's not accidentally used on the IO thread.
    void* browser_state;
  };

  explicit ChromeBrowserStateIOData(
      ios::ChromeBrowserStateType browser_state_type);

  void InitializeOnUIThread(ios::ChromeBrowserState* browser_state);
  void ApplyProfileParamsToContext(net::URLRequestContext* context) const;

  // Called when the ChromeBrowserState is destroyed. |context_getters| must
  // include all URLRequestContextGetters that refer to the
  // ChromeBrowserStateIOData's URLRequestContexts. Triggers destruction of the
  // ChromeBrowserStateIOData and shuts down |context_getters| safely on the IO
  // thread.
  // TODO(mmenke):  Passing all those URLRequestContextGetters around like this
  //     is really silly.  Can we do something cleaner?
  void ShutdownOnUIThread(
      std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters);

  net::ProxyResolutionService* proxy_resolution_service() const {
    return proxy_resolution_service_.get();
  }

  net::HttpServerProperties* http_server_properties() const;

  void set_http_server_properties(
      std::unique_ptr<net::HttpServerProperties> http_server_properties) const;

  net::URLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  bool initialized() const { return initialized_; }

  std::unique_ptr<net::HttpNetworkSession> CreateHttpNetworkSession(
      const ProfileParams& profile_params) const;

  // Creates main network transaction factory.
  std::unique_ptr<net::HttpCache> CreateMainHttpFactory(
      net::HttpNetworkSession* session,
      std::unique_ptr<net::HttpCache::BackendFactory> main_backend) const;

  // Creates network transaction factory.
  std::unique_ptr<net::HttpCache> CreateHttpFactory(
      net::HttpNetworkSession* shared_session,
      std::unique_ptr<net::HttpCache::BackendFactory> backend) const;

 private:
  typedef std::map<base::FilePath, AppRequestContext*> URLRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ChromeBrowserStateIOData subtype.
  // Subtypes should use the static helper functions above to implement this.
  virtual void InitializeInternal(
      std::unique_ptr<IOSChromeNetworkDelegate> chrome_network_delegate,
      ProfileParams* profile_params,
      ProtocolHandlerMap* protocol_handlers) const = 0;

  // The order *DOES* matter for the majority of these member variables, so
  // don't move them around unless you know what you're doing!
  // General rules:
  //   * ResourceContext references the URLRequestContexts, so
  //   URLRequestContexts must outlive ResourceContext, hence ResourceContext
  //   should be destroyed first.
  //   * URLRequestContexts reference a whole bunch of members, so
  //   URLRequestContext needs to be destroyed before them.
  //   * Therefore, ResourceContext should be listed last, and then the
  //   URLRequestContexts, and then the URLRequestContext members.
  //   * Note that URLRequestContext members have a directed dependency graph
  //   too, so they must themselves be ordered correctly.

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the ChromeBrowserState, used to initialize
  // ChromeBrowserStateIOData. Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember enable_do_not_track_;

  BooleanPrefMember enable_metrics_;

  mutable std::unique_ptr<net::ProxyResolutionService>
      proxy_resolution_service_;
  mutable std::unique_ptr<net::TransportSecurityState>
      transport_security_state_;
  mutable std::unique_ptr<net::CTVerifier> cert_transparency_verifier_;
  mutable std::unique_ptr<net::HttpServerProperties> http_server_properties_;
  mutable std::unique_ptr<net::TransportSecurityPersister>
      transport_security_persister_;
  mutable std::unique_ptr<net::ReportSender> certificate_report_sender_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable std::unique_ptr<net::URLRequestContext> main_request_context_;
  // One URLRequestContext per isolated app for main and media requests.
  mutable URLRequestContextMap app_request_context_map_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  mutable std::unique_ptr<IOSChromeHttpUserAgentSettings>
      chrome_http_user_agent_settings_;

  const ios::ChromeBrowserStateType browser_state_type_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserStateIOData);
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IO_DATA_H_
