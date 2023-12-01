// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/application_context/model/application_context_impl.h"

#import <algorithm>
#import <vector>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "base/time/default_tick_clock.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/timer_update_scheduler.h"
#import "components/gcm_driver/gcm_client_factory.h"
#import "components/gcm_driver/gcm_desktop_utils.h"
#import "components/gcm_driver/gcm_driver.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/net_log/net_export_file_writer.h"
#import "components/network_time/network_time_tracker.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/core/session_id_generator.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/ukm/ukm_service.h"
#import "components/update_client/configurator.h"
#import "components/update_client/update_query_params.h"
#import "components/variations/service/variations_service.h"
#import "components/version_info/channel.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"
#import "ios/chrome/browser/browser_state/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/component_updater/model/ios_component_updater_configurator.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/application_breadcrumbs_logger.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_services_manager_client.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"
#import "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/segmentation_platform/model/otr_web_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/update_client/model/ios_chrome_update_query_params_delegate.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service_impl.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "net/log/net_log.h"
#import "net/log/net_log_capture_mode.h"
#import "net/socket/client_socket_pool_manager.h"
#import "net/url_request/url_request_context_getter.h"
#import "services/metrics/public/cpp/ukm_recorder.h"
#import "services/network/network_change_manager.h"
#import "services/network/public/cpp/network_connection_tracker.h"
#import "services/network/public/mojom/network_service.mojom.h"
#import "ui/base/resource/resource_bundle.h"

namespace {

// Requests a network::mojom::ProxyResolvingSocketFactory on the UI thread.
// Note that this cannot be called on a thread that is not the UI thread.
void RequestProxyResolvingSocketFactoryOnUIThread(
    ApplicationContextImpl* app_context,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  network::mojom::NetworkContext* network_context =
      app_context->GetSystemNetworkContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

// Wrapper on top of the method above. This does a PostTask to the UI thread.
void RequestProxyResolvingSocketFactory(
    ApplicationContextImpl* app_context,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                app_context, std::move(receiver)));
}

// Passed to NetworkConnectionTracker to bind a NetworkChangeManager receiver.
void BindNetworkChangeManagerReceiver(
    network::NetworkChangeManager* network_change_manager,
    mojo::PendingReceiver<network::mojom::NetworkChangeManager> receiver) {
  network_change_manager->AddReceiver(std::move(receiver));
}

}  // namespace

ApplicationContextImpl::ApplicationContextImpl(
    base::SequencedTaskRunner* local_state_task_runner,
    const base::CommandLine& command_line,
    const std::string& locale,
    const std::string& country)
    : local_state_task_runner_(local_state_task_runner) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);

  SetApplicationLocale(locale);
  application_country_ = country;

  update_client::UpdateQueryParams::SetDelegate(
      IOSChromeUpdateQueryParamsDelegate::GetInstance());
}

ApplicationContextImpl::~ApplicationContextImpl() {
  DCHECK_EQ(this, GetApplicationContext());
  SetApplicationContext(nullptr);
}

void ApplicationContextImpl::PreCreateThreads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ios_chrome_io_thread_.reset(
      new IOSChromeIOThread(GetLocalState(), GetNetLog()));
}

void ApplicationContextImpl::PostCreateThreads() {
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                base::Unretained(ios_chrome_io_thread_.get())));
}

void ApplicationContextImpl::PreMainMessageLoopRun() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // BrowserPolicyConnectorIOS is created very early because local_state()
  // needs policy to be initialized with the managed preference values.
  // However, policy fetches from the network and loading of disk caches
  // requires that threads are running; this Init() call lets the connector
  // resume its initialization now that the loops are spinning and the
  // system request context is available for the fetchers.
  BrowserPolicyConnectorIOS* browser_policy_connector =
      GetBrowserPolicyConnector();
  if (browser_policy_connector) {
    browser_policy_connector->Init(GetLocalState(),
                                   GetSharedURLLoaderFactory());
  }

  if (breadcrumbs::MaybeEnableBasedOnChannel(GetLocalState(), ::GetChannel())) {
    // Start crash reporter listening for breadcrumb events. Collected
    // breadcrumbs will be attached to crash reports.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();

    base::FilePath storage_dir;
    bool result = base::PathService::Get(ios::DIR_USER_DATA, &storage_dir);
    DCHECK(result);
    application_breadcrumbs_logger_ =
        std::make_unique<ApplicationBreadcrumbsLogger>(storage_dir);
  }
}

void ApplicationContextImpl::StartTearDown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Destroy the segmentation OTR observer before
  // `chrome_browser_state_manager_`. `segmentation_otr_web_state_observer_` may
  // not be initialized when segmentation platform feature is disabled
  if (segmentation_otr_web_state_observer_) {
    segmentation_otr_web_state_observer_->TearDown();
  }

  // We need to destroy the MetricsServicesManager and NetworkTimeTracker before
  // the IO thread gets destroyed, since the destructor can call the URLFetcher
  // destructor, which does a PostDelayedTask operation on the IO thread. (The
  // IO thread will handle that URLFetcher operation before going away.)
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service) {
    metrics_service->LogCleanShutdown();
  }
  metrics_services_manager_.reset();
  network_time_tracker_.reset();

  net_export_file_writer_.reset();

  if (safe_browsing_service_) {
    safe_browsing_service_->ShutDown();
  }

  // Need to clear browser states before the IO thread.
  chrome_browser_state_manager_.reset();

  // The policy providers managed by `browser_policy_connector_` need to shut
  // down while the IO threads is still alive. The monitoring framework owned by
  // `browser_policy_connector_` relies on `gcm_driver_`, so this must be
  // shutdown before `gcm_driver_` below.
  if (browser_policy_connector_) {
    browser_policy_connector_->Shutdown();
  }

  // The GCMDriver must shut down while the IO thread is still alive.
  if (gcm_driver_) {
    gcm_driver_->Shutdown();
  }

  if (local_state_) {
    local_state_->CommitPendingWrite();
    sessions::SessionIdGenerator::GetInstance()->Shutdown();
  }

  // The ApplicationBreadcrumbsLogger tries to log event via a task when it
  // is destroyed, so it needs to be notified of the app tear down now when
  // the task tracker is still valid (will be destroyed after StartTearDown
  // returns).
  application_breadcrumbs_logger_.reset();

  ios_chrome_io_thread_->NetworkTearDown();
}

void ApplicationContextImpl::PostDestroyThreads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Resets associated state right after actual thread is stopped as
  // IOSChromeIOThread::Globals cleanup happens in CleanUp on the IO
  // thread, i.e. as the thread exits its message loop.
  //
  // This is important because in various places, the IOSChromeIOThread
  // object being NULL is considered synonymous with the IO thread
  // having stopped.
  ios_chrome_io_thread_.reset();
}

void ApplicationContextImpl::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Tell the metrics services that the application resumes.
  PrefService* local_state = GetLocalState();
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service && local_state) {
    metrics_service->OnAppEnterForeground();
    local_state->CommitPendingWrite();
  }

  variations::VariationsService* variations_service = GetVariationsService();
  if (variations_service) {
    variations_service->OnAppEnterForeground();
  }
  ukm::UkmService* ukm_service = GetMetricsServicesManager()->GetUkmService();
  if (ukm_service) {
    ukm_service->OnAppEnterForeground();
  }
}

void ApplicationContextImpl::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Mark all the ChromeBrowserStates as clean and persist history.
  std::vector<ChromeBrowserState*> loaded_browser_state =
      GetChromeBrowserStateManager()->GetLoadedBrowserStates();
  for (ChromeBrowserState* browser_state : loaded_browser_state) {
    if (history::HistoryService* history_service =
            ios::HistoryServiceFactory::GetForBrowserStateIfExists(
                browser_state, ServiceAccessType::EXPLICIT_ACCESS)) {
      history_service->HandleBackgrounding();
    }

    PrefService* browser_state_prefs = browser_state->GetPrefs();
    if (browser_state_prefs) {
      browser_state_prefs->CommitPendingWrite();
    }
  }

  // Tell the metrics services they were cleanly shutdown.
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service) {
    metrics_service->OnAppEnterBackground(
        /*keep_recording_in_background=*/true);
  }
  ukm::UkmService* ukm_service = GetMetricsServicesManager()->GetUkmService();
  if (ukm_service) {
    ukm_service->OnAppEnterBackground();
  }

  // Persisting to disk is protected by a critical task, so no other special
  // handling is necessary on iOS.
}

bool ApplicationContextImpl::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service) {
    return metrics_service->WasLastShutdownClean();
  }
  return true;
}

PrefService* ApplicationContextImpl::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!local_state_) {
    CreateLocalState();
  }
  return local_state_.get();
}

net::URLRequestContextGetter*
ApplicationContextImpl::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios_chrome_io_thread_->system_url_request_context_getter();
}

scoped_refptr<network::SharedURLLoaderFactory>
ApplicationContextImpl::GetSharedURLLoaderFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios_chrome_io_thread_->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
ApplicationContextImpl::GetSystemNetworkContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios_chrome_io_thread_->GetSystemNetworkContext();
}

const std::string& ApplicationContextImpl::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

const std::string& ApplicationContextImpl::GetApplicationCountry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_country_.empty());
  return application_country_;
}

ios::ChromeBrowserStateManager*
ApplicationContextImpl::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!chrome_browser_state_manager_) {
    chrome_browser_state_manager_.reset(new ChromeBrowserStateManagerImpl());
  }
  return chrome_browser_state_manager_.get();
}

metrics_services_manager::MetricsServicesManager*
ApplicationContextImpl::GetMetricsServicesManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_services_manager_) {
    metrics_services_manager_.reset(
        new metrics_services_manager::MetricsServicesManager(
            std::make_unique<IOSChromeMetricsServicesManagerClient>(
                GetLocalState())));
  }
  return metrics_services_manager_.get();
}

metrics::MetricsService* ApplicationContextImpl::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetMetricsService();
}

ukm::UkmRecorder* ApplicationContextImpl::GetUkmRecorder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetUkmService();
}

variations::VariationsService* ApplicationContextImpl::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetVariationsService();
}

net::NetLog* ApplicationContextImpl::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return net::NetLog::Get();
}

net_log::NetExportFileWriter* ApplicationContextImpl::GetNetExportFileWriter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!net_export_file_writer_) {
    net_export_file_writer_ = std::make_unique<net_log::NetExportFileWriter>();
  }
  return net_export_file_writer_.get();
}

network_time::NetworkTimeTracker*
ApplicationContextImpl::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        base::WrapUnique(new base::DefaultClock),
        base::WrapUnique(new base::DefaultTickClock), GetLocalState(),
        GetSharedURLLoaderFactory()));
  }
  return network_time_tracker_.get();
}

IOSChromeIOThread* ApplicationContextImpl::GetIOSChromeIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ios_chrome_io_thread_.get());
  return ios_chrome_io_thread_.get();
}

gcm::GCMDriver* ApplicationContextImpl::GetGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!gcm_driver_) {
    CreateGCMDriver();
  }
  DCHECK(gcm_driver_);
  return gcm_driver_.get();
}

component_updater::ComponentUpdateService*
ApplicationContextImpl::GetComponentUpdateService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!component_updater_) {
    // Creating the component updater does not do anything, components need to
    // be registered and Start() needs to be called.
    component_updater_ = component_updater::ComponentUpdateServiceFactory(
        component_updater::MakeIOSComponentUpdaterConfigurator(
            base::CommandLine::ForCurrentProcess()),
        std::make_unique<component_updater::TimerUpdateScheduler>(),
        ios::provider::GetBrandCode());
  }
  return component_updater_.get();
}

SafeBrowsingService* ApplicationContextImpl::GetSafeBrowsingService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!safe_browsing_service_) {
    safe_browsing_service_ = base::MakeRefCounted<SafeBrowsingServiceImpl>();
  }
  return safe_browsing_service_.get();
}

network::NetworkConnectionTracker*
ApplicationContextImpl::GetNetworkConnectionTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_connection_tracker_) {
    DCHECK(!network_change_manager_);
    network_change_manager_ =
        std::make_unique<network::NetworkChangeManager>(nullptr);
    network_connection_tracker_ =
        std::make_unique<network::NetworkConnectionTracker>(base::BindRepeating(
            &BindNetworkChangeManagerReceiver,
            base::Unretained(network_change_manager_.get())));
  }
  return network_connection_tracker_.get();
}

BrowserPolicyConnectorIOS* ApplicationContextImpl::GetBrowserPolicyConnector() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!browser_policy_connector_.get()) {
    // Ensure that the ResourceBundle has already been initialized. If this
    // DCHECK ever fails, a call to
    // BrowserPolicyConnector::OnResourceBundleCreated() will need to be added
    // later in the startup sequence, after the ResourceBundle is initialized.
    DCHECK(ui::ResourceBundle::HasSharedInstance());
    version_info::Channel channel = ::GetChannel();
    policy::ConfigurationPolicyProvider* test_policy_provider =
        tests_hook::GetOverriddenPlatformPolicyProvider();

    // If running under test (for example, if a mock platform policy provider
    // was provided), enable future policies without requiring them to be on
    // an allowlist. Otherwise, disable future policies on
    // externally-published channels, unless a domain administrator explicitly
    // adds them to the allowlist.
    bool enable_future_policies_without_allowlist =
        test_policy_provider != nullptr ||
        (channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA);
    browser_policy_connector_ =
        std::make_unique<BrowserPolicyConnectorIOS>(base::BindRepeating(
            &BuildPolicyHandlerList, enable_future_policies_without_allowlist));

    // Install a mock platform policy provider, if running under EG2 and one
    // is supplied.
    if (test_policy_provider) {
      browser_policy_connector_->SetPolicyProviderForTesting(  // IN-TEST
          test_policy_provider);
    }
  }
  return browser_policy_connector_.get();
}

id<SingleSignOnService> ApplicationContextImpl::GetSSOService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!single_sign_on_service_) {
    single_sign_on_service_ = ios::provider::CreateSSOService();
    DCHECK(single_sign_on_service_);
  }
  return single_sign_on_service_;
}

SystemIdentityManager* ApplicationContextImpl::GetSystemIdentityManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!system_identity_manager_) {
    // Give the opportunity for the test hook to override the factory from
    // the provider (allowing EG tests to use a fake SystemIdentityManager).
    system_identity_manager_ = tests_hook::CreateSystemIdentityManager();
    if (!system_identity_manager_) {
      system_identity_manager_ =
          ios::provider::CreateSystemIdentityManager(GetSSOService());
    }
    DCHECK(system_identity_manager_);
  }
  return system_identity_manager_.get();
}

segmentation_platform::OTRWebStateObserver*
ApplicationContextImpl::GetSegmentationOTRWebStateObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!segmentation_otr_web_state_observer_) {
    segmentation_otr_web_state_observer_ =
        std::make_unique<segmentation_platform::OTRWebStateObserver>(
            GetChromeBrowserStateManager());
  }
  return segmentation_otr_web_state_observer_.get();
}

PushNotificationService* ApplicationContextImpl::GetPushNotificationService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!push_notification_service_) {
    push_notification_service_ = ios::provider::CreatePushNotificationService();
    DCHECK(push_notification_service_);
  }

  return push_notification_service_.get();
}

void ApplicationContextImpl::SetApplicationLocale(const std::string& locale) {
  DCHECK(thread_checker_.CalledOnValidThread());
  application_locale_ = locale;
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      application_locale_);
}

void ApplicationContextImpl::CreateLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!local_state_);

  base::FilePath local_state_path;
  CHECK(base::PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));

  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple);

  // Register local state preferences.
  RegisterLocalStatePrefs(pref_registry.get());

  policy::BrowserPolicyConnector* browser_policy_connector =
      GetBrowserPolicyConnector();
  policy::PolicyService* policy_service =
      browser_policy_connector ? browser_policy_connector->GetPolicyService()
                               : nullptr;
  local_state_ = ::CreateLocalState(
      local_state_path, local_state_task_runner_.get(), pref_registry,
      policy_service, browser_policy_connector);
  DCHECK(local_state_);

  sessions::SessionIdGenerator::GetInstance()->Init(local_state_.get());

  net::ClientSocketPoolManager::set_max_sockets_per_proxy_chain(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::max(std::min<int>(net::kDefaultMaxSocketsPerProxyChain, 99),
               net::ClientSocketPoolManager::max_sockets_per_group(
                   net::HttpNetworkSession::NORMAL_SOCKET_POOL)));

  // Cleanup obsolete preferences.
  MigrateObsoleteLocalStatePrefs(local_state_.get());

  // Delete obsolete data from user storage.
  CleanupUnusedStorage();

  // Delete obsolete data from NSUserDefaults.
  MigrateObsoleteUserDefault();
}

void ApplicationContextImpl::CreateGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!gcm_driver_);

  base::FilePath store_path;
  CHECK(base::PathService::Get(ios::DIR_GLOBAL_GCM_STORE, &store_path));

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  gcm_driver_ = gcm::CreateGCMDriverDesktop(
      base::WrapUnique(new gcm::GCMClientFactory), GetLocalState(), store_path,
      // Because ApplicationContextImpl is destroyed after all WebThreads have
      // been shut down, base::Unretained() is safe here.
      base::BindRepeating(&RequestProxyResolvingSocketFactory,
                          base::Unretained(this)),
      GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker(), ::GetChannel(),
      IOSChromeGCMProfileServiceFactory::GetProductCategoryForSubtypes(),
      web::GetUIThreadTaskRunner({}), web::GetIOThreadTaskRunner({}),
      blocking_task_runner);
}
