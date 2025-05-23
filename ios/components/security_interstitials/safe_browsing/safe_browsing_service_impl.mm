// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service_impl.h"

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "build/branding_buildflags.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "components/safe_browsing/core/browser/url_checker_delegate.h"
#import "components/safe_browsing/core/browser/utils/url_loader_factory_params.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safebrowsing_constants.h"
#import "components/sessions/core/session_id.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "net/cookies/cookie_store.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_builder.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#pragma mark - SafeBrowsingServiceImpl

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr std::string_view kClientName = "googlechrome";
#else
inline constexpr std::string_view kClientName = "chromium";
#endif

}  // namespace

SafeBrowsingServiceImpl::SafeBrowsingServiceImpl() {
  url_loader_factory_pending_receiver_ =
      url_loader_factory_.BindNewPipeAndPassReceiver();
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          url_loader_factory_.get());
}

SafeBrowsingServiceImpl::~SafeBrowsingServiceImpl() = default;

void SafeBrowsingServiceImpl::Initialize(const base::FilePath& user_data_path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(!io_thread_enabler_);

  base::FilePath safe_browsing_data_path =
      user_data_path.Append(safe_browsing::kSafeBrowsingBaseFilename);
  safe_browsing_db_manager_ = safe_browsing::V4LocalDatabaseManager::Create(
      safe_browsing_data_path, web::GetUIThreadTaskRunner({}),
      web::GetIOThreadTaskRunner({}),
      safe_browsing::ExtendedReportingLevelCallback());

  io_thread_enabler_ = base::MakeRefCounted<IOThreadEnabler>();

  std::string user_agent =
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadEnabler::Initialize, io_thread_enabler_,
                     network_context_client_.BindNewPipeAndPassReceiver(),
                     std::move(safe_browsing_data_path),
                     std::move(user_agent)));

  network_context_client_->CreateURLLoaderFactory(
      std::move(url_loader_factory_pending_receiver_),
      safe_browsing::GetUrlLoaderFactoryParams());
}

void SafeBrowsingServiceImpl::OnBrowserStateCreated(
    PrefService* prefs,
    safe_browsing::SafeBrowsingMetricsCollector* metrics_collector) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // Watch for changes to the Safe Browsing opt-out preference.
  auto registrar = std::make_unique<PrefChangeRegistrar>();
  registrar->Init(prefs);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::BindRepeating(
                     &SafeBrowsingServiceImpl::UpdateSafeBrowsingEnabledState,
                     base::Unretained(this)));

  auto insertion_result = pref_change_registrars_.insert(
      std::make_pair(prefs, std::move(registrar)));
  DCHECK(insertion_result.second);

  UMA_HISTOGRAM_BOOLEAN(safe_browsing::kSafeBrowsingEnabledHistogramName,
                        prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
  // TODO(crbug.com/40886668): Deprecate SafeBrowsing.Pref.Enhanced.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Enhanced",
                        prefs->GetBoolean(prefs::kSafeBrowsingEnhanced));
  // TODO(crbug.com/332512508): We will need to update the
  // SafeBrowsing.Pref.Enhanced.RegularProfile metric to support multi-profile.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Enhanced.RegularProfile",
                        prefs->GetBoolean(prefs::kSafeBrowsingEnhanced));
  safe_browsing::RecordExtendedReportingMetrics(*prefs);
  if (base::FeatureList::IsEnabled(
          safe_browsing::kExtendedReportingRemovePrefDependency)) {
    prefs->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated,
        prefs->GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  } else {
    prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated,
                      false);
  }
  UpdateSafeBrowsingEnabledState();

  if (metrics_collector) {
    metrics_collector->StartLogging();
  }
}

void SafeBrowsingServiceImpl::OnBrowserStateDestroyed(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // OnBrowserStateDestroyed(...) may be called after ShutDown(). In that
  // case, `prefs` will no longer be in `pref_change_registrars_` and the
  // call can be ignored.
  auto iter = pref_change_registrars_.find(prefs);
  if (iter != pref_change_registrars_.end()) {
    pref_change_registrars_.erase(iter);
    UpdateSafeBrowsingEnabledState();
  }
}

void SafeBrowsingServiceImpl::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  pref_change_registrars_.clear();
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadEnabler::ShutDown, io_thread_enabler_));
  if (enabled_) {
    enabled_ = false;
    safe_browsing_db_manager_->StopOnUIThread(true);
  }
  network_context_client_.reset();
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
SafeBrowsingServiceImpl::CreateUrlChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service =
      client->GetRealTimeUrlLookupService();
  bool can_perform_full_url_lookup =
      url_lookup_service && url_lookup_service->CanPerformFullURLLookup();
  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate =
      base::MakeRefCounted<UrlCheckerDelegateImpl>(safe_browsing_db_manager_,
                                                   client->AsWeakPtr());
  safe_browsing::HashRealTimeService* hash_real_time_service =
      client->GetHashRealTimeService();

  safe_browsing::hash_realtime_utils::HashRealTimeSelection
      hash_real_time_selection =
          safe_browsing::hash_realtime_utils::DetermineHashRealTimeSelection(
              web_state->GetBrowserState()->IsOffTheRecord(),
              client->GetPrefs(),
              safe_browsing::hash_realtime_utils::GetCountryCode(
                  client->GetVariationsService()),
              /*log_usage_histograms=*/true,
              /*are_background_lookups_allowed=*/false);

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service_ is null, safe browsing database should be checked by
  // default.
  bool can_check_db = can_perform_full_url_lookup
                          ? url_lookup_service->CanCheckSafeBrowsingDb()
                          : true;
  bool can_check_high_confidence_allowlist =
      can_perform_full_url_lookup
          ? url_lookup_service->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;
  std::string url_lookup_service_metric_suffix =
      can_perform_full_url_lookup ? url_lookup_service->GetMetricSuffix()
                                  : safe_browsing::kNoRealTimeURLLookupService;

  return std::make_unique<safe_browsing::SafeBrowsingUrlCheckerImpl>(
      /*headers=*/net::HttpRequestHeaders(), /*load_flags=*/0,
      /*has_user_gesture=*/false, url_checker_delegate,
      /*web_contents_getter=*/
      base::RepeatingCallback<content::WebContents*()>(),
      web_state->GetWeakPtr(),
      /*render_process_id=*/
      security_interstitials::UnsafeResource::kNoRenderProcessId,
      /*render_frame_token=*/std::nullopt,
      /*frame_tree_node_id=*/
      security_interstitials::UnsafeResource::kNoFrameTreeNodeId,
      /*navigation_id=*/std::nullopt, can_perform_full_url_lookup, can_check_db,
      can_check_high_confidence_allowlist, url_lookup_service_metric_suffix,
      web::GetUIThreadTaskRunner({}),
      url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr,
      hash_real_time_service ? hash_real_time_service->GetWeakPtr() : nullptr,
      hash_real_time_selection,
      /*is_async_check=*/false, /*check_allowlist_before_hash_database=*/false,
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
SafeBrowsingServiceImpl::CreateAsyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service =
      client->GetRealTimeUrlLookupService();
  bool can_perform_full_url_lookup =
      url_lookup_service && url_lookup_service->CanPerformFullURLLookup();
  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate =
      base::MakeRefCounted<UrlCheckerDelegateImpl>(safe_browsing_db_manager_,
                                                   client->AsWeakPtr());
  safe_browsing::HashRealTimeService* hash_real_time_service =
      client->GetHashRealTimeService();

  safe_browsing::hash_realtime_utils::HashRealTimeSelection
      hash_real_time_selection =
          safe_browsing::hash_realtime_utils::DetermineHashRealTimeSelection(
              web_state->GetBrowserState()->IsOffTheRecord(),
              client->GetPrefs(),
              safe_browsing::hash_realtime_utils::GetCountryCode(
                  client->GetVariationsService()),
              /*log_usage_histograms=*/true,
              /*are_background_lookups_allowed=*/false);

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service_ is null, safe browsing database should be checked by
  // default.
  bool can_check_db = can_perform_full_url_lookup
                          ? url_lookup_service->CanCheckSafeBrowsingDb()
                          : true;
  bool can_check_high_confidence_allowlist =
      can_perform_full_url_lookup
          ? url_lookup_service->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;
  std::string url_lookup_service_metric_suffix =
      can_perform_full_url_lookup ? url_lookup_service->GetMetricSuffix()
                                  : safe_browsing::kNoRealTimeURLLookupService;

  return std::make_unique<safe_browsing::SafeBrowsingUrlCheckerImpl>(
      /*headers=*/net::HttpRequestHeaders(), /*load_flags=*/0,
      /*has_user_gesture=*/false, url_checker_delegate,
      /*web_contents_getter=*/
      base::RepeatingCallback<content::WebContents*()>(),
      web_state->GetWeakPtr(),
      /*render_process_id=*/
      security_interstitials::UnsafeResource::kNoRenderProcessId,
      /*render_frame_token=*/std::nullopt,
      /*frame_tree_node_id=*/
      security_interstitials::UnsafeResource::kNoFrameTreeNodeId,
      /*navigation_id=*/std::nullopt, can_perform_full_url_lookup, can_check_db,
      can_check_high_confidence_allowlist, url_lookup_service_metric_suffix,
      web::GetUIThreadTaskRunner({}),
      url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr,
      hash_real_time_service ? hash_real_time_service->GetWeakPtr() : nullptr,
      hash_real_time_selection,
      /*is_async_check=*/true, /*check_allowlist_before_hash_database=*/false,
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
SafeBrowsingServiceImpl::CreateSyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate =
      base::MakeRefCounted<UrlCheckerDelegateImpl>(safe_browsing_db_manager_,
                                                   client->AsWeakPtr());

  return std::make_unique<safe_browsing::SafeBrowsingUrlCheckerImpl>(
      /*headers=*/net::HttpRequestHeaders(), /*load_flags=*/0,
      /*has_user_gesture=*/false, url_checker_delegate,
      /*web_contents_getter=*/
      base::RepeatingCallback<content::WebContents*()>(),
      web_state->GetWeakPtr(),
      /*render_process_id=*/
      security_interstitials::UnsafeResource::kNoRenderProcessId,
      /*render_frame_token=*/std::nullopt,
      /*frame_tree_node_id=*/
      security_interstitials::UnsafeResource::kNoFrameTreeNodeId,
      /*navigation_id=*/std::nullopt, /*url_real_time_lookup_enabled=*/false,
      /*can_check_db=*/true, /*can_check_high_confidence_allowlist=*/true,
      /*url_lookup_service_metric_suffix=*/"", web::GetUIThreadTaskRunner({}),
      /*url_lookup_service=*/nullptr,
      /*hash_realtime_service=*/nullptr,
      /*hash_realtime_selection=*/
      safe_browsing::hash_realtime_utils::HashRealTimeSelection::kNone,
      /*is_async_check=*/false, /*check_allowlist_before_hash_database=*/false,
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);
}

// Checks if async check should be created.
bool SafeBrowsingServiceImpl::ShouldCreateAsyncChecker(
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  if (!web_state) {
    return false;
  }

  if (client->ShouldForceSyncRealTimeUrlChecks()) {
    return false;
  }

  safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service =
      client->GetRealTimeUrlLookupService();
  bool can_perform_full_url_lookup =
      url_lookup_service && url_lookup_service->CanPerformFullURLLookup();

  safe_browsing::hash_realtime_utils::HashRealTimeSelection
      hash_real_time_selection =
          safe_browsing::hash_realtime_utils::DetermineHashRealTimeSelection(
              web_state->GetBrowserState()->IsOffTheRecord(),
              client->GetPrefs(),
              safe_browsing::hash_realtime_utils::GetCountryCode(
                  client->GetVariationsService()),
              /*log_usage_histograms=*/true,
              /*are_background_lookups_allowed=*/false);

  if (!can_perform_full_url_lookup &&
      hash_real_time_selection ==
          safe_browsing::hash_realtime_utils::HashRealTimeSelection::kNone) {
    return false;
  }

  return true;
}

bool SafeBrowsingServiceImpl::CanCheckUrl(const GURL& url) const {
  return safe_browsing_db_manager_->CanCheckUrl(url);
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingServiceImpl::GetURLLoaderFactory() {
  return shared_url_loader_factory_;
}

scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
SafeBrowsingServiceImpl::GetDatabaseManager() {
  return safe_browsing_db_manager_;
}

network::mojom::NetworkContext* SafeBrowsingServiceImpl::GetNetworkContext() {
  return network_context_client_.get();
}

void SafeBrowsingServiceImpl::ClearCookies(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    base::OnceClosure callback) {
  if (creation_range.start() == base::Time() &&
      creation_range.end() == base::Time::Max()) {
    web::GetIOThreadTaskRunner({base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&IOThreadEnabler::ClearAllCookies,
                                  io_thread_enabler_, std::move(callback)));
  } else {
    web::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }
}

void SafeBrowsingServiceImpl::SetUpURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  url_loader_factory_->Clone(std::move(receiver));
}

void SafeBrowsingServiceImpl::UpdateSafeBrowsingEnabledState() {
  bool enabled = false;
  for (const auto& [prefs, _] : pref_change_registrars_) {
    if (prefs->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      enabled = true;
      break;
    }
  }
  if (enabled_ == enabled) {
    return;
  }

  enabled_ = enabled;
  if (enabled_) {
    safe_browsing_db_manager_->StartOnUIThread(
        shared_url_loader_factory_,
        safe_browsing::GetV4ProtocolConfig(std::string(kClientName),
                                           /*disable_auto_update=*/false));
  } else {
    safe_browsing_db_manager_->StopOnUIThread(false);
  }
}

#pragma mark - SafeBrowsingServiceImpl::IOThreadEnabler

SafeBrowsingServiceImpl::IOThreadEnabler::IOThreadEnabler() = default;

SafeBrowsingServiceImpl::IOThreadEnabler::~IOThreadEnabler() = default;

void SafeBrowsingServiceImpl::IOThreadEnabler::Initialize(
    mojo::PendingReceiver<network::mojom::NetworkContext>
        network_context_receiver,
    const base::FilePath& safe_browsing_data_path,
    const std::string& user_agent) {
  SetUpURLRequestContext(safe_browsing_data_path, user_agent);
  std::vector<std::string> cors_exempt_header_list;
  network_context_ = std::make_unique<network::NetworkContext>(
      /*network_service=*/nullptr, std::move(network_context_receiver),
      url_request_context_.get(), cors_exempt_header_list);
}

void SafeBrowsingServiceImpl::IOThreadEnabler::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  network_context_.reset();
  url_request_context_.reset();
}

void SafeBrowsingServiceImpl::IOThreadEnabler::ClearAllCookies(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  net::CookieStore* cookie_store = url_request_context_->cookie_store();
  cookie_store->DeleteAllAsync(base::BindOnce(
      [](base::OnceClosure callback, uint32_t) { std::move(callback).Run(); },
      std::move(callback)));
}

void SafeBrowsingServiceImpl::IOThreadEnabler::SetUpURLRequestContext(
    const base::FilePath& safe_browsing_data_path,
    const std::string& user_agent) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  net::URLRequestContextBuilder builder;
  base::FilePath cookie_file_path(safe_browsing_data_path.value() +
                                  safe_browsing::kCookiesFile);
  std::unique_ptr<net::CookieStore> cookie_store =
      cookie_util::CreateCookieStore(
          cookie_util::CookieStoreConfig(
              cookie_file_path,
              cookie_util::CookieStoreConfig::RESTORED_SESSION_COOKIES,
              cookie_util::CookieStoreConfig::COOKIE_MONSTER),
          /*system_cookie_store=*/nullptr, net::NetLog::Get());

  builder.SetCookieStore(std::move(cookie_store));
  builder.set_user_agent(user_agent);
  url_request_context_ = builder.Build();
}
