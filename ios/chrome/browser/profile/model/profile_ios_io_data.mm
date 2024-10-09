// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_ios_io_data.h"

#import <stddef.h>

#import <string>
#import <utility>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/containers/contains.h"
#import "base/debug/alias.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/content_settings/core/browser/content_settings_provider.h"
#import "components/content_settings/core/browser/cookie_settings.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/net_log/chrome_net_log.h"
#import "components/prefs/pref_service.h"
#import "components/proxy_config/ios/proxy_service_factory.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/net/model/accept_language_pref_watcher.h"
#import "ios/chrome/browser/net/model/ios_chrome_http_user_agent_settings.h"
#import "ios/chrome/browser/net/model/ios_chrome_network_delegate.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/profile/model/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/public/browsing_data/system_cookie_store_util.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cert/cert_verifier.h"
#import "net/cookies/canonical_cookie.h"
#import "net/http/http_auth_handler_factory.h"
#import "net/http/http_network_session.h"
#import "net/http/http_transaction_factory.h"
#import "net/http/http_util.h"
#import "net/http/transport_security_persister.h"
#import "net/nqe/network_quality_estimator.h"
#import "net/proxy_resolution/pac_file_fetcher_impl.h"
#import "net/proxy_resolution/proxy_config_service_fixed.h"
#import "net/proxy_resolution/proxy_resolution_service.h"
#import "net/quic/quic_context.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "net/url_request/url_request.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_builder.h"
#import "net/url_request/url_request_job_factory.h"

namespace {

// For safe shutdown, must be called before the ProfileIOSIOData is
// destroyed.
void NotifyContextGettersOfShutdownOnIO(
    std::unique_ptr<
        ProfileIOSIOData::IOSChromeURLRequestContextGetterVector>
        getters) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  for (auto& chrome_context_getter : *getters) {
    chrome_context_getter->NotifyContextShuttingDown();
  }
}

}  // namespace

void ProfileIOSIOData::InitializeOnUIThread(ProfileIOS* profile) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  PrefService* pref_service = profile->GetPrefs();
  auto params = std::make_unique<ProfileParams>();
  params->path = profile->GetOriginalProfile()->GetStatePath();

  params->io_thread = GetApplicationContext()->GetIOSChromeIOThread();

  params->cookie_settings = ios::CookieSettingsFactory::GetForProfile(profile);
  params->host_content_settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(profile);

  params->proxy_config_service = ProxyServiceFactory::CreateProxyConfigService(
      profile->GetProxyConfigTracker());
  params->system_cookie_store = web::CreateSystemCookieStore(profile);
  params->profile = profile;
  profile_params_ = std::move(params);

  IOSChromeNetworkDelegate::InitializePrefsOnUIThread(&enable_do_not_track_,
                                                      pref_service);

  accept_language_pref_watcher_ =
      std::make_unique<AcceptLanguagePrefWatcher>(pref_service);
  chrome_http_user_agent_settings_ =
      std::make_unique<IOSChromeHttpUserAgentSettings>(
          accept_language_pref_watcher_->GetHandle());
}

ProfileIOSIOData::ProfileParams::ProfileParams()
    : io_thread(nullptr), profile(nullptr) {}

ProfileIOSIOData::ProfileParams::~ProfileParams() {}

ProfileIOSIOData::ProfileIOSIOData(ProfileIOSType profile_type)
    : initialized_(false), profile_type_(profile_type) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

ProfileIOSIOData::~ProfileIOSIOData() {
  if (web::WebThread::IsThreadInitialized(web::WebThread::IO)) {
    DCHECK_CURRENTLY_ON(web::WebThread::IO);
  }
}

net::URLRequestContext* ProfileIOSIOData::GetMainRequestContext()
    const {
  DCHECK(initialized_);
  return main_request_context_.get();
}

content_settings::CookieSettings* ProfileIOSIOData::GetCookieSettings()
    const {
  DCHECK(initialized_);
  return cookie_settings_.get();
}

HostContentSettingsMap* ProfileIOSIOData::GetHostContentSettingsMap()
    const {
  DCHECK(initialized_);
  return host_content_settings_map_.get();
}

bool ProfileIOSIOData::IsOffTheRecord() const {
  return profile_type() == ProfileIOSType::INCOGNITO_PROFILE;
}

void ProfileIOSIOData::InitializeMetricsEnabledStateOnUIThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Prep the PrefMember and send it to the IO thread, since this value will be
  // read from there.
  enable_metrics_.Init(metrics::prefs::kMetricsReportingEnabled,
                       GetApplicationContext()->GetLocalState());
  enable_metrics_.MoveToSequence(web::GetIOThreadTaskRunner({}));
}

bool ProfileIOSIOData::GetMetricsEnabledStateOnIOThread() const {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return enable_metrics_.GetValue();
}

void ProfileIOSIOData::Init(
    ProtocolHandlerMap* protocol_handlers) const {
  // The basic logic is implemented here. The specific initialization
  // is done in InitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!initialized_);
  DCHECK(profile_params_.get());

  IOSChromeIOThread* const io_thread = profile_params_->io_thread;
  IOSChromeIOThread::Globals* const io_thread_globals = io_thread->globals();

  net::URLRequestContextBuilder context_builder;
  context_builder.set_net_log(io_thread->net_log());
  auto network_delegate = std::make_unique<IOSChromeNetworkDelegate>();
  network_delegate->set_cookie_settings(profile_params_->cookie_settings.get());
  network_delegate->set_enable_do_not_track(&enable_do_not_track_);
  context_builder.set_network_delegate(std::move(network_delegate));
  auto quic_context = std::make_unique<net::QuicContext>();
  *quic_context->params() = io_thread->quic_params();
  context_builder.set_quic_context(std::move(quic_context));

  // NOTE: The proxy resolution service uses the default io thread network
  // delegate, not the delegate just created.
  context_builder.set_proxy_resolution_service(
      ProxyServiceFactory::CreateProxyResolutionService(
          io_thread->net_log(), nullptr,
          io_thread_globals->system_request_context->network_delegate(),
          std::move(profile_params_->proxy_config_service),
          true /* quick_check_enabled */));
  if (!IsOffTheRecord()) {
    context_builder.set_transport_security_persister_file_path(
        profile_params_->path.Append(FILE_PATH_LITERAL("TransportSecurity")));
  }

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
  host_content_settings_map_ = profile_params_->host_content_settings_map;

  context_builder.SetHttpAuthHandlerFactory(
      io_thread->CreateHttpAuthHandlerFactory());
  context_builder.set_http_user_agent_settings(
      std::move(chrome_http_user_agent_settings_));
  context_builder.set_http_network_session_params(
      io_thread->NetworkSessionParams());

  for (auto& pair : *protocol_handlers) {
    const std::string& scheme = pair.first;
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler> handler =
        std::move(pair.second);
    context_builder.SetProtocolHandler(scheme, std::move(handler));
  }
  protocol_handlers->clear();

  InitializeInternal(&context_builder, profile_params_.get());

  main_request_context_ = context_builder.Build();

  profile_params_.reset();
  initialized_ = true;
}

void ProfileIOSIOData::ShutdownOnUIThread(
    std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  enable_referrers_.Destroy();
  enable_do_not_track_.Destroy();
  enable_metrics_.Destroy();
  accept_language_pref_watcher_.reset();

  if (!context_getters->empty()) {
    if (web::WebThread::IsThreadInitialized(web::WebThread::IO)) {
      web::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&NotifyContextGettersOfShutdownOnIO,
                                    std::move(context_getters)));
    }
  }

  bool posted = web::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  if (!posted) {
    delete this;
  }
}
