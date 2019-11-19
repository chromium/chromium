// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_IO_THREAD_IOS_IO_THREAD_H_
#define IOS_COMPONENTS_IO_THREAD_IOS_IO_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_member.h"
#include "ios/web/public/thread/web_thread_delegate.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_session.h"

class PrefProxyConfigTracker;
class PrefService;

namespace net {
class CTPolicyEnforcer;
class CertVerifier;
class CookieStore;
class CTVerifier;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpAuthPreferences;
class HttpServerProperties;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class LoggingNetworkChangeObserver;
class NetLog;
class NetworkDelegate;
class ProxyConfigService;
class ProxyResolutionService;
class SSLConfigService;
class TransportSecurityState;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestJobFactory;
}  // namespace net

namespace io_thread {

class SystemURLRequestContextGetter;

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
//
// If you are looking to interact with the IO thread (e.g. post tasks
// to it or check if it is the current thread), see web::WebThread.
class IOSIOThread : public web::WebThreadDelegate {
 public:
  struct Globals {
    template <typename T>
    class Optional {
     public:
      Optional() : set_(false) {}

      void set(T value) {
        set_ = true;
        value_ = value;
      }
      void CopyToIfSet(T* value) const {
        if (set_) {
          *value = value_;
        }
      }

     private:
      bool set_;
      T value_;
    };

    class SystemRequestContextLeakChecker {
     public:
      explicit SystemRequestContextLeakChecker(Globals* globals);
      ~SystemRequestContextLeakChecker();

     private:
      Globals* const globals_;
    };

    Globals();
    ~Globals();

    // The "system" NetworkDelegate, used for BrowserState-agnostic network
    // events.
    std::unique_ptr<net::NetworkDelegate> system_network_delegate;
    std::unique_ptr<net::HostResolver> host_resolver;
    std::unique_ptr<net::CertVerifier> cert_verifier;
    // This TransportSecurityState doesn't load or save any state. It's only
    // used to enforce pinning for system requests and will only use built-in
    // pins.
    std::unique_ptr<net::TransportSecurityState> transport_security_state;
    std::unique_ptr<net::CTVerifier> cert_transparency_verifier;
    std::unique_ptr<net::SSLConfigService> ssl_config_service;
    std::unique_ptr<net::HttpAuthPreferences> http_auth_preferences;
    std::unique_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    std::unique_ptr<net::HttpServerProperties> http_server_properties;
    std::unique_ptr<net::ProxyResolutionService> system_proxy_resolution_service;
    std::unique_ptr<net::QuicContext> quic_context;
    std::unique_ptr<net::HttpNetworkSession> system_http_network_session;
    std::unique_ptr<net::HttpTransactionFactory>
        system_http_transaction_factory;
    std::unique_ptr<net::URLRequestJobFactory> system_url_request_job_factory;
    std::unique_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
    std::unique_ptr<net::CookieStore> system_cookie_store;
    std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings;
    std::unique_ptr<net::CTPolicyEnforcer> ct_policy_enforcer;
  };

  // |net_log| must either outlive the IOSIOThread or be NULL.
  IOSIOThread(PrefService* local_state, net::NetLog* net_log);
  ~IOSIOThread() override;

  // Can only be called on the IO thread.
  Globals* globals();

  // Allows overriding Globals in tests where IOSIOThread::Init() and
  // IOSIOThread::CleanUp() are not called.  This allows for injecting mocks
  // into IOSIOThread global objects.
  void SetGlobalsForTesting(Globals* globals);

  net::NetLog* net_log();

  // Handles changing to On The Record mode, discarding confidential data.
  void ChangedToOnTheRecord();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Clears the host cache.  Intended to be used to prevent exposing recently
  // visited sites on about:net-internals/#dns and about:dns pages.  Must be
  // called on the IO thread.
  void ClearHostCache();

  const net::HttpNetworkSession::Params& NetworkSessionParams() const;

 protected:
  // A string describing the current application version. For example: "stable"
  // or "dev". An empty string is an acceptable value. This string is used to
  // build the user agent id for QUIC.
  virtual std::string GetChannelString() const = 0;

  // Subclasses must return a NetworkDelegate to be used for
  // BrowserState-agnostic network events.
  virtual std::unique_ptr<net::NetworkDelegate>
  CreateSystemNetworkDelegate() = 0;

 private:
  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  // WebThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  void Init() override;
  void CleanUp() override;

  // Sets up HttpAuthPreferences and HttpAuthHandlerFactory on Globals.
  void CreateDefaultAuthHandlerFactory();

  // Discards confidential data. To be called on IO thread only.
  void ChangedToOnTheRecordOnIOThread();

  static net::URLRequestContext* ConstructSystemRequestContext(
      Globals* globals,
      const net::HttpNetworkSession::Params& params,
      net::NetLog* net_log);

  // The NetLog is owned by the application context, to allow logging from other
  // threads during shutdown, but is used most frequently on the IO thread.
  net::NetLog* net_log_;

  // These member variables are basically global, but their lifetimes are tied
  // to the IOSIOThread.  IOSIOThread owns them all, despite not using
  // scoped_ptr. This is because the destructor of IOSIOThread runs on the
  // wrong thread.  All member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  net::HttpNetworkSession::Params params_;

  // Observer that logs network changes to the NetLog.
  std::unique_ptr<net::LoggingNetworkChangeObserver> network_change_observer_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOSIOThread.
  std::unique_ptr<net::ProxyConfigService> system_proxy_config_service_;

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_refptr<SystemURLRequestContextGetter>
      system_url_request_context_getter_;

  base::WeakPtrFactory<IOSIOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSIOThread);
};

}  // namespace io_thread

#endif  // IOS_COMPONENTS_IO_THREAD_IO_THREAD_H_
