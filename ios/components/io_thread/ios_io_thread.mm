// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/io_thread/ios_io_thread.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/ios/proxy_service_factory.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/components/io_thread/leak_tracker.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/web_client.h"
#include "net/base/logging_network_change_observer.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The IOSIOThread object must outlive any tasks posted to the IO thread before
// the Quit task, so base::Bind{Once,Repeating}() calls are not refcounted.

namespace io_thread {

namespace {

const char kSupportedAuthSchemes[] = "basic,digest,ntlm";

}  // namespace

std::unique_ptr<net::HostResolver> CreateGlobalHostResolver(
    net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOSIOThread::CreateGlobalHostResolver");

  // TODO(crbug.com/934402): Use a shared HostResolverManager instead of a
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
  IOSIOThread* io_thread_;  // Weak pointer, owned by ApplicationContext.
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

// |local_state| is passed in explicitly in order to (1) reduce implicit
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

  std::string quic_user_agent_id = GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(web::BuildOSCpuInfo());

  // Set up field trials, ignoring debug command line options.
  network_session_configurator::ParseCommandLineAndFieldTrials(
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      /*is_quic_force_disabled=*/false, quic_user_agent_id, &params_,
      &quic_params_);

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
  // TODO(crbug.com/801910): Hook up logging by passing in a non-null netlog.
  builder.SetCookieStore(std::make_unique<net::CookieMonster>(
      nullptr /* store */, nullptr /* netlog */, net::kFirstPartySetsEnabled));
  builder.set_network_delegate(std::move(network_delegate));
  builder.set_user_agent(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
  builder.set_http_network_session_params(params_);
  return builder.Build();
}

}  // namespace io_thread
