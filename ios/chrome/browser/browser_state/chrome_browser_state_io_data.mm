// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_io_data.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/ios/proxy_service_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/net/ios_chrome_http_user_agent_settings.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#import "ios/net/cookies/system_cookie_store.h"
#include "ios/web/public/browsing_data/system_cookie_store_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_persister.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// For safe shutdown, must be called before the ChromeBrowserStateIOData is
// destroyed.
void NotifyContextGettersOfShutdownOnIO(
    std::unique_ptr<
        ChromeBrowserStateIOData::IOSChromeURLRequestContextGetterVector>
        getters) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  for (auto& chrome_context_getter : *getters)
    chrome_context_getter->NotifyContextShuttingDown();
}

}  // namespace

void ChromeBrowserStateIOData::InitializeOnUIThread(
    ios::ChromeBrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  PrefService* pref_service = browser_state->GetPrefs();
  std::unique_ptr<ProfileParams> params(new ProfileParams);
  params->path = browser_state->GetOriginalChromeBrowserState()->GetStatePath();

  params->io_thread = GetApplicationContext()->GetIOSChromeIOThread();

  params->cookie_settings =
      ios::CookieSettingsFactory::GetForBrowserState(browser_state);
  params->host_content_settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state);

  params->proxy_config_service = ProxyServiceFactory::CreateProxyConfigService(
      browser_state->GetProxyConfigTracker());
  params->system_cookie_store = web::CreateSystemCookieStore(browser_state);
  params->browser_state = browser_state;
  profile_params_.reset(params.release());

  IOSChromeNetworkDelegate::InitializePrefsOnUIThread(&enable_do_not_track_,
                                                      pref_service);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunner({web::WebThread::IO});

  chrome_http_user_agent_settings_.reset(
      new IOSChromeHttpUserAgentSettings(pref_service));
}

ChromeBrowserStateIOData::AppRequestContext::AppRequestContext() {}

void ChromeBrowserStateIOData::AppRequestContext::SetCookieStore(
    std::unique_ptr<net::CookieStore> cookie_store) {
  cookie_store_ = std::move(cookie_store);
  set_cookie_store(cookie_store_.get());
}

void ChromeBrowserStateIOData::AppRequestContext::SetHttpNetworkSession(
    std::unique_ptr<net::HttpNetworkSession> http_network_session) {
  http_network_session_ = std::move(http_network_session);
}

void ChromeBrowserStateIOData::AppRequestContext::SetHttpTransactionFactory(
    std::unique_ptr<net::HttpTransactionFactory> http_factory) {
  http_factory_ = std::move(http_factory);
  set_http_transaction_factory(http_factory_.get());
}

void ChromeBrowserStateIOData::AppRequestContext::SetJobFactory(
    std::unique_ptr<net::URLRequestJobFactory> job_factory) {
  job_factory_ = std::move(job_factory);
  set_job_factory(job_factory_.get());
}

ChromeBrowserStateIOData::AppRequestContext::~AppRequestContext() {
  AssertNoURLRequests();
}

ChromeBrowserStateIOData::ProfileParams::ProfileParams()
    : io_thread(nullptr), browser_state(nullptr) {}

ChromeBrowserStateIOData::ProfileParams::~ProfileParams() {}

ChromeBrowserStateIOData::ChromeBrowserStateIOData(
    ios::ChromeBrowserStateType browser_state_type)
    : initialized_(false), browser_state_type_(browser_state_type) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

ChromeBrowserStateIOData::~ChromeBrowserStateIOData() {
  if (web::WebThread::IsThreadInitialized(web::WebThread::IO))
    DCHECK_CURRENTLY_ON(web::WebThread::IO);

  // Pull the contents of the request context maps onto the stack for sanity
  // checking of values in a minidump. http://crbug.com/260425
  size_t num_app_contexts = app_request_context_map_.size();
  size_t current_context = 0;
  static const size_t kMaxCachedContexts = 20;
  net::URLRequestContext* app_context_cache[kMaxCachedContexts] = {0};
  void* app_context_vtable_cache[kMaxCachedContexts] = {0};
  void* tmp_vtable = nullptr;
  base::debug::Alias(&num_app_contexts);
  base::debug::Alias(&current_context);
  base::debug::Alias(app_context_cache);
  base::debug::Alias(app_context_vtable_cache);
  base::debug::Alias(&tmp_vtable);

  current_context = 0;
  for (URLRequestContextMap::const_iterator
           it = app_request_context_map_.begin();
       current_context < kMaxCachedContexts &&
       it != app_request_context_map_.end();
       ++it, ++current_context) {
    app_context_cache[current_context] = it->second;
    memcpy(&app_context_vtable_cache[current_context],
           static_cast<void*>(it->second), sizeof(void*));
  }

  // Destroy certificate_report_sender_ before main_request_context_,
  // since the former has a reference to the latter.
  if (transport_security_state_)
    transport_security_state_->SetReportSender(nullptr);
  certificate_report_sender_.reset();

  // TODO(crbug.com/787061): These AssertNoURLRequests() calls are unnecessary
  // since they are already done in the URLRequestContext destructor.
  if (main_request_context_)
    main_request_context_->AssertNoURLRequests();

  current_context = 0;
  for (URLRequestContextMap::iterator it = app_request_context_map_.begin();
       it != app_request_context_map_.end(); ++it) {
    if (current_context < kMaxCachedContexts) {
      CHECK_EQ(app_context_cache[current_context], it->second);
      memcpy(&tmp_vtable, static_cast<void*>(it->second), sizeof(void*));
      CHECK_EQ(app_context_vtable_cache[current_context], tmp_vtable);
    }
    it->second->AssertNoURLRequests();
    delete it->second;
    current_context++;
  }
}

// static
void ChromeBrowserStateIOData::InstallProtocolHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    ProtocolHandlerMap* protocol_handlers) {
  for (ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end(); ++it) {
    bool set_protocol = job_factory->SetProtocolHandler(
        it->first, base::WrapUnique(it->second.release()));
    DCHECK(set_protocol);
  }
  protocol_handlers->clear();
}

net::URLRequestContext* ChromeBrowserStateIOData::GetMainRequestContext()
    const {
  DCHECK(initialized_);
  return main_request_context_.get();
}

void ChromeBrowserStateIOData::SetCookieStoreForPartitionPath(
    std::unique_ptr<net::CookieStore> cookie_store,
    const base::FilePath& partition_path) {
  DCHECK(base::Contains(app_request_context_map_, partition_path));
  app_request_context_map_[partition_path]->SetCookieStore(
      std::move(cookie_store));
}

content_settings::CookieSettings* ChromeBrowserStateIOData::GetCookieSettings()
    const {
  DCHECK(initialized_);
  return cookie_settings_.get();
}

HostContentSettingsMap* ChromeBrowserStateIOData::GetHostContentSettingsMap()
    const {
  DCHECK(initialized_);
  return host_content_settings_map_.get();
}

bool ChromeBrowserStateIOData::IsOffTheRecord() const {
  return browser_state_type() ==
         ios::ChromeBrowserStateType::INCOGNITO_BROWSER_STATE;
}

void ChromeBrowserStateIOData::InitializeMetricsEnabledStateOnUIThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Prep the PrefMember and send it to the IO thread, since this value will be
  // read from there.
  enable_metrics_.Init(metrics::prefs::kMetricsReportingEnabled,
                       GetApplicationContext()->GetLocalState());
  enable_metrics_.MoveToSequence(
      base::CreateSingleThreadTaskRunner({web::WebThread::IO}));
}

bool ChromeBrowserStateIOData::GetMetricsEnabledStateOnIOThread() const {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return enable_metrics_.GetValue();
}

net::HttpServerProperties* ChromeBrowserStateIOData::http_server_properties()
    const {
  return http_server_properties_.get();
}

void ChromeBrowserStateIOData::set_http_server_properties(
    std::unique_ptr<net::HttpServerProperties> http_server_properties) const {
  http_server_properties_ = std::move(http_server_properties);
}

void ChromeBrowserStateIOData::Init(
    ProtocolHandlerMap* protocol_handlers) const {
  // The basic logic is implemented here. The specific initialization
  // is done in InitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!initialized_);
  DCHECK(profile_params_.get());

  IOSChromeIOThread* const io_thread = profile_params_->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  // Create the common request contexts.
  main_request_context_.reset(new net::URLRequestContext());

  std::unique_ptr<IOSChromeNetworkDelegate> network_delegate(
      new IOSChromeNetworkDelegate());

  network_delegate->set_cookie_settings(profile_params_->cookie_settings.get());
  network_delegate->set_enable_do_not_track(&enable_do_not_track_);

  // NOTE: The proxy resolution service uses the default io thread network
  // delegate, not the delegate just created.
  proxy_resolution_service_ = ProxyServiceFactory::CreateProxyResolutionService(
      io_thread->net_log(), nullptr,
      io_thread_globals->system_network_delegate.get(),
      std::move(profile_params_->proxy_config_service),
      true /* quick_check_enabled */);
  transport_security_state_.reset(new net::TransportSecurityState());
  if (!IsOffTheRecord()) {
    transport_security_persister_ =
        std::make_unique<net::TransportSecurityPersister>(
            transport_security_state_.get(), profile_params_->path,
            base::CreateSequencedTaskRunner(
                {base::ThreadPool(), base::MayBlock(),
                 base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("domain_security_policy", R"(
        semantics {
          sender: "Domain Security Policy"
          description:
            "Websites can opt in to have Chrome send reports to them when "
            "Chrome observes connections to that website that do not meet "
            "stricter security policies, such as with HTTP Public Key Pinning. "
            "Websites can use this feature to discover misconfigurations that "
            "prevent them from complying with stricter security policies that "
            "they've opted in to."
          trigger:
            "Chrome observes that a user is loading a resource from a website "
            "that has opted in for security policy reports, and the connection "
            "does not meet the required security policies."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and sometimes certificate revocation "
            "information included on the connection."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, this is a feature that websites can opt into and "
            "thus there is no Chrome-wide policy to disable it."
        })");
  certificate_report_sender_ = std::make_unique<net::ReportSender>(
      main_request_context_.get(), traffic_annotation);
  transport_security_state_->SetReportSender(certificate_report_sender_.get());

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
  host_content_settings_map_ = profile_params_->host_content_settings_map;

  main_request_context_->set_ssl_config_service(
      io_thread_globals->ssl_config_service.get());
  main_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_request_context_->set_ct_policy_enforcer(
      io_thread_globals->ct_policy_enforcer.get());
  main_request_context_->set_cert_transparency_verifier(
      io_thread_globals->cert_transparency_verifier.get());
  main_request_context_->set_quic_context(
      io_thread_globals->quic_context.get());

  InitializeInternal(std::move(network_delegate), profile_params_.get(),
                     protocol_handlers);

  profile_params_.reset();
  initialized_ = true;
}

void ChromeBrowserStateIOData::ApplyProfileParamsToContext(
    net::URLRequestContext* context) const {
  context->set_http_user_agent_settings(chrome_http_user_agent_settings_.get());
}

void ChromeBrowserStateIOData::ShutdownOnUIThread(
    std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  enable_referrers_.Destroy();
  enable_do_not_track_.Destroy();
  enable_metrics_.Destroy();
  if (chrome_http_user_agent_settings_)
    chrome_http_user_agent_settings_->CleanupOnUIThread();

  if (!context_getters->empty()) {
    if (web::WebThread::IsThreadInitialized(web::WebThread::IO)) {
      base::PostTask(FROM_HERE, {web::WebThread::IO},
                     base::BindOnce(&NotifyContextGettersOfShutdownOnIO,
                                    std::move(context_getters)));
    }
  }

  bool posted = base::DeleteSoon(FROM_HERE, {web::WebThread::IO}, this);
  if (!posted)
    delete this;
}

std::unique_ptr<net::HttpNetworkSession>
ChromeBrowserStateIOData::CreateHttpNetworkSession(
    const ProfileParams& profile_params) const {
  net::URLRequestContext* context = main_request_context();

  IOSChromeIOThread* const io_thread = profile_params.io_thread;

  net::HttpNetworkSession::Context session_context;
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &session_context);

  return std::unique_ptr<net::HttpNetworkSession>(new net::HttpNetworkSession(
      io_thread->NetworkSessionParams(), session_context));
}

std::unique_ptr<net::HttpCache> ChromeBrowserStateIOData::CreateMainHttpFactory(
    net::HttpNetworkSession* session,
    std::unique_ptr<net::HttpCache::BackendFactory> main_backend) const {
  return std::unique_ptr<net::HttpCache>(
      new net::HttpCache(session, std::move(main_backend), true));
}

std::unique_ptr<net::HttpCache> ChromeBrowserStateIOData::CreateHttpFactory(
    net::HttpNetworkSession* shared_session,
    std::unique_ptr<net::HttpCache::BackendFactory> backend) const {
  return std::unique_ptr<net::HttpCache>(
      new net::HttpCache(shared_session, std::move(backend), true));
}
