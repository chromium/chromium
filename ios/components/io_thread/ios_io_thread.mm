// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/io_thread/ios_io_thread.h"

#import <stddef.h>

#import <utility>
#import <vector>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/environment.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/field_trial.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "base/task/single_thread_task_runner.h"
#import "base/threading/thread.h"
#import "base/threading/thread_restrictions.h"
#import "base/time/time.h"
#import "base/trace_event/trace_event.h"
#import "components/network_session_configurator/browser/network_session_configurator.h"
#import "components/prefs/pref_service.h"
#import "components/proxy_config/ios/proxy_service_factory.h"
#import "components/proxy_config/pref_proxy_config_tracker.h"
#import "components/variations/variations_associated_data.h"
#import "components/version_info/version_info.h"
#import "ios/components/io_thread/leak_tracker.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "net/base/logging_network_change_observer.h"
#import "net/cert/cert_verifier.h"
#import "net/cert/ct_policy_enforcer.h"
#import "net/cert/multi_threaded_cert_verifier.h"
#import "net/cookies/cookie_monster.h"
#import "net/cookies/cookie_store.h"
#import "net/dns/host_cache.h"
#import "net/dns/host_resolver.h"
#import "net/http/http_auth_filter.h"
#import "net/http/http_auth_handler_factory.h"
#import "net/http/http_auth_preferences.h"
#import "net/http/http_network_layer.h"
#import "net/http/http_server_properties.h"
#import "net/http/transport_security_state.h"
#import "net/log/net_log.h"
#import "net/log/net_log_event_type.h"
#import "net/proxy_resolution/pac_file_fetcher_impl.h"
#import "net/proxy_resolution/proxy_config_service.h"
#import "net/proxy_resolution/proxy_resolution_service.h"
#import "net/quic/quic_context.h"
#import "net/socket/tcp_client_socket.h"
#import "net/spdy/spdy_session.h"
#import "net/ssl/ssl_config_service_defaults.h"
#import "net/url_request/static_http_user_agent_settings.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_builder.h"
#import "net/url_request/url_request_context_getter.h"
#import "net/url_request/url_request_job_factory.h"
#import "url/url_constants.h"

// The IOSIOThread object must outlive any tasks posted to the IO thread before
// the Quit task, so base::Bind{Once,Repeating}() calls are not refcounted.

namespace io_thread {

namespace {

const char kSupportedAuthSchemes[] = "basic,digest,ntlm";

}  // namespace

std::unique_ptr<net::HostResolver> CreateGlobalHostResolver(
    net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOSIOThread::CreateGlobalHostResolver");

  // TODO(crbug.com/40614970): Use a shared HostResolverManager instead of a
  // single global HostResolver for iOS.
  std::unique_ptr<net::HostResolver> global_host_resolver =
      net::HostResolver::CreateStandaloneResolver(net_log);

  return global_host_resolver;
}

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOSIOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Tells the getter that the URLRequestContext is about to be shut down.
  void Shutdown();

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  raw_ptr<IOSIOThread>
      io_thread_;  // Weak pointer, owned by ApplicationContext.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOSIOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(web::GetIOThreadTaskRunner({})) {}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!io_thread_)
    return nullptr;
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void SystemURLRequestContextGetter::Shutdown() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  io_thread_ = nullptr;
  NotifyContextShuttingDown();
}

IOSIOThread::Globals::SystemRequestContextLeakChecker::
    SystemRequestContextLeakChecker(Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOSIOThread::Globals::SystemRequestContextLeakChecker::
    ~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOSIOThread::Globals::Globals() : system_request_context_leak_checker(this) {}

IOSIOThread::Globals::~Globals() {}

// `local_state` is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOSIOThread more flexible for testing.
IOSIOThread::IOSIOThread(PrefService* local_state, net::NetLog* net_log)
    : net_log_(net_log), globals_(nullptr), weak_factory_(this) {
  pref_proxy_config_tracker_ =
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state);
  system_proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get());

  web::WebThread::SetIOThreadDelegate(this);
}

IOSIOThread::~IOSIOThread() {
  // This isn't needed for production code, but in tests, IOSIOThread may
  // be multiply constructed.
  web::WebThread::SetIOThreadDelegate(nullptr);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOSIOThread::Globals* IOSIOThread::globals() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return globals_;
}

void IOSIOThread::InitOnIO() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  // Allow blocking calls while initializing the IO thread.
  base::ScopedAllowBlocking allow_blocking_for_init;
  Init();
}

void IOSIOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

net::NetLog* IOSIOThread::net_log() {
  return net_log_;
}

void IOSIOThread::ChangedToOnTheRecord() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOSIOThread::ChangedToOnTheRecordOnIOThread,
                                base::Unretained(this)));
}

net::URLRequestContextGetter* IOSIOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!system_url_request_context_getter_.get()) {
    // If we're in unit_tests, IOSIOThread may not be run.
    if (!web::WebThread::IsThreadInitialized(web::WebThread::IO))
      return nullptr;
    system_url_request_context_getter_ =
        new SystemURLRequestContextGetter(this);
  }
  return system_url_request_context_getter_.get();
}

void IOSIOThread::Init() {
  TRACE_EVENT0("startup", "IOSIOThread::Init");
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the NetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_ =
      std::make_unique<net::LoggingNetworkChangeObserver>(net_log_);

  CreateDefaultAuthPreferences();

  params_.ignore_certificate_errors = false;
  params_.enable_user_alternate_protocol_ports = false;

  // Set up field trials, ignoring debug command line options.
  network_session_configurator::ParseCommandLineAndFieldTrials(
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      /*is_quic_force_disabled=*/false, &params_, &quic_params_);

  globals_->system_request_context = ConstructSystemRequestContext();
}

void IOSIOThread::CleanUp() {
  system_url_request_context_getter_->Shutdown();
  system_url_request_context_getter_ = nullptr;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // This must be reset before the NetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = nullptr;

  LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOSIOThread::CreateDefaultAuthPreferences() {
  std::vector<std::string> supported_schemes =
      base::SplitString(kSupportedAuthSchemes, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences =
      std::make_unique<net::HttpAuthPreferences>();
  globals_->http_auth_preferences->set_allowed_schemes(std::set<std::string>(
      supported_schemes.begin(), supported_schemes.end()));
}

void IOSIOThread::ClearHostCache() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  net::HostCache* host_cache =
      globals_->system_request_context->host_resolver()->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

const net::HttpNetworkSessionParams& IOSIOThread::NetworkSessionParams() const {
  return params_;
}

std::unique_ptr<net::HttpAuthHandlerFactory>
IOSIOThread::CreateHttpAuthHandlerFactory() {
  return net::HttpAuthHandlerRegistryFactory::Create(
      globals_->http_auth_preferences.get());
}

void IOSIOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

std::unique_ptr<net::URLRequestContext>
IOSIOThread::ConstructSystemRequestContext() {
  net::URLRequestContextBuilder builder;

  auto network_delegate = CreateSystemNetworkDelegate();
  builder.set_net_log(net_log_);
  builder.set_host_resolver(CreateGlobalHostResolver(net_log_));
  builder.SetHttpAuthHandlerFactory(CreateHttpAuthHandlerFactory());
  builder.set_proxy_resolution_service(
      ProxyServiceFactory::CreateProxyResolutionService(
          net_log_, nullptr, network_delegate.get(),
          std::move(system_proxy_config_service_),
          true /* quick_check_enabled */));
  auto quic_context = std::make_unique<net::QuicContext>();
  *quic_context->params() = quic_params_;
  builder.set_quic_context(std::move(quic_context));
  // In-memory cookie store.
  // TODO(crbug.com/41364708): Hook up logging by passing in a non-null netlog.
  builder.SetCookieStore(std::make_unique<net::CookieMonster>(
      nullptr /* store */, nullptr /* netlog */));
  builder.set_network_delegate(std::move(network_delegate));
  builder.set_user_agent(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
  builder.set_http_network_session_params(params_);
  return builder.Build();
}

}  // namespace io_thread
