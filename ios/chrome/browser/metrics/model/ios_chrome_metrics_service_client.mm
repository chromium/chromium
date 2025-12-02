// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_client.h"

#import <UIKit/UIKit.h>
#import <stdint.h>

#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/check.h"
#import "base/command_line.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/device_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/persistent_histogram_allocator.h"
#import "base/path_service.h"
#import "base/process/process_metrics.h"
#import "base/rand_util.h"
#import "base/strings/safe_sprintf.h"
#import "base/task/thread_pool.h"
#import "base/threading/platform_thread.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/country_codes/country_codes.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/crash_keys.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#import "components/metrics/cpu_metrics_provider.h"
#import "components/metrics/demographics/demographic_metrics_provider.h"
#import "components/metrics/drive_metrics_provider.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "components/metrics/dwa/dwa_service.h"
#import "components/metrics/entropy_state_provider.h"
#import "components/metrics/field_trials_provider.h"
#import "components/metrics/install_date_provider.h"
#import "components/metrics/metrics_data_validation.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/net/cellular_logic_helper.h"
#import "components/metrics/net/net_metrics_log_uploader.h"
#import "components/metrics/net/network_metrics_provider.h"
#import "components/metrics/persistent_histograms.h"
#import "components/metrics/private_metrics/puma_service.h"
#import "components/metrics/stability_metrics_helper.h"
#import "components/metrics/ui/form_factor_metrics_provider.h"
#import "components/metrics/ui/screen_info_metrics_provider.h"
#import "components/metrics/version_utils.h"
#import "components/omnibox/browser/omnibox_metrics_provider.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_country_id.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/sync/service/passphrase_type_metrics_provider.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_count_metrics_provider.h"
#import "components/ukm/field_trials_provider_helper.h"
#import "components/ukm/ukm_service.h"
#import "components/variations/synthetic_trial_registry.h"
#import "components/variations/variations_associated_data.h"
#import "components/version_info/version_info.h"
#import "google_apis/google_api_keys.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/demographics_client.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_signin_and_sync_status_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_stability_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_family_link_user_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_feed_activity_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"
#import "ios/chrome/browser/metrics/model/mobile_session_shutdown_metrics_provider.h"
#import "ios/chrome/browser/regional_capabilities/model/ios_regional_capabilities_metrics_provider.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/incognito_session_tracker.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/translate/model/translate_ranker_metrics_provider.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Maximum amount of local storage for storing persistent histograms.
const int kMaxHistogramStorageKiB = 50 << 10;  // 50 MiB

void GetNetworkConnectionTrackerAsync(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  std::move(callback).Run(
      GetApplicationContext()->GetNetworkConnectionTracker());
}

std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    bool metrics_reporting_enabled) {
  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(GetApplicationContext()->GetLocalState(),
                                       IsFirstRun()));

  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    metrics::FileMetricsProvider::Params browser_metrics_params(
        user_data_dir.AppendASCII(kBrowserMetricsName),
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
        metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
        kBrowserMetricsName);
    browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
    browser_metrics_params.filter = base::BindRepeating(
        &IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles);
    file_metrics_provider->RegisterSource(browser_metrics_params,
                                          metrics_reporting_enabled);
  }
  return file_metrics_provider;
}

}  // namespace

IOSChromeMetricsServiceClient::IOSChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager,
    variations::SyntheticTrialRegistry* synthetic_trial_registry)
    : UkmConsentStateObserver(ukm::NoInitialUkmConsentState),
      metrics_state_manager_(state_manager),
      synthetic_trial_registry_(synthetic_trial_registry),
      stability_metrics_provider_(nullptr) {
  RegisterForNotifications();

  // The IncognitoSessionTracker may be null during unit tests.
  if (IncognitoSessionTracker* tracker =
          GetApplicationContext()->GetIncognitoSessionTracker()) {
    // Using base::Unretained(this) is safe since destroying the subscription
    // will invalidate the callback and the subscription is owned by this.
    incognito_session_tracker_subscription_ =
        tracker->RegisterCallback(base::IgnoreArgs<bool>(base::BindRepeating(
            &IOSChromeMetricsServiceClient::UpdateRunningServices,
            base::Unretained(this))));
  }
}

IOSChromeMetricsServiceClient::~IOSChromeMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<IOSChromeMetricsServiceClient>
IOSChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  // Perform two-phase initialization so that `client->metrics_service_` only
  // receives pointers to fully constructed objects.
  std::unique_ptr<IOSChromeMetricsServiceClient> client(
      new IOSChromeMetricsServiceClient(state_manager,
                                        synthetic_trial_registry));
  client->Initialize();

  return client;
}

// static
void IOSChromeMetricsServiceClient::RegisterPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  metrics::FileMetricsProvider::RegisterSourcePrefs(registry,
                                                    kBrowserMetricsName);
  metrics::RegisterMetricsReportingStatePrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
  metrics::dwa::DwaService::RegisterPrefs(registry);
  metrics::private_metrics::PumaService::RegisterPrefs(registry);
}

variations::SyntheticTrialRegistry*
IOSChromeMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* IOSChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* IOSChromeMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

metrics::dwa::DwaService* IOSChromeMetricsServiceClient::GetDwaService() {
  return dwa_service_.get();
}

metrics::private_metrics::PumaService*
IOSChromeMetricsServiceClient::GetPumaService() {
  return puma_service_.get();
}

std::optional<regional_capabilities::CountryIdHolder>
IOSChromeMetricsServiceClient::GetProfileCountryIdForPrivateMetricsReporting() {
  std::vector<ProfileIOS*> profiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  if (profiles.empty()) {
    return std::nullopt;
  }

  std::optional<regional_capabilities::CountryIdHolder> first_country_id;
  for (ProfileIOS* profile : profiles) {
    if (profile->IsOffTheRecord()) {
      continue;
    }
    regional_capabilities::RegionalCapabilitiesService* service =
        ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
    if (!service) {
      continue;
    }

    regional_capabilities::CountryIdHolder current_id = service->GetCountryId();

    if (!first_country_id.has_value()) {
      first_country_id = current_id;
    } else if (first_country_id.value() != current_id) {
      // Mismatched IDs.
      return std::nullopt;
    }
  }

  return first_country_id;
}

void IOSChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

int32_t IOSChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string IOSChromeMetricsServiceClient::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocaleStorage()->Get();
}

const network_time::NetworkTimeTracker*
IOSChromeMetricsServiceClient::GetNetworkTimeTracker() {
  return GetApplicationContext()->GetNetworkTimeTracker();
}

bool IOSChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  brand_code->assign(ios::provider::GetBrandCode());
  return true;
}

metrics::SystemProfileProto::Channel
IOSChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(::GetChannel());
}

bool IOSChromeMetricsServiceClient::IsExtendedStableChannel() {
  return false;  // Not supported on iOS.
}

std::string IOSChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void IOSChromeMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  collect_final_metrics_done_callback_ = std::move(done_callback);
  CollectFinalHistograms();
}

std::unique_ptr<metrics::MetricsLogUploader>
IOSChromeMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<metrics::NetMetricsLogUploader>(
      GetApplicationContext()->GetSharedURLLoaderFactory(), server_url,
      insecure_server_url, mime_type, service_type, on_upload_complete);
}

base::TimeDelta IOSChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval(metrics::ShouldUseCellularUploadInterval());
}

void IOSChromeMetricsServiceClient::Initialize() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  synthetic_trial_observation_.Observe(synthetic_trial_registry_.get());

  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_, this, local_state);
  RegisterMetricsServiceProviders();

  if (IsMetricsReportingForceEnabled() ||
      base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    ukm_service_ = std::make_unique<ukm::UkmService>(
        local_state, this,
        std::make_unique<metrics::DemographicMetricsProvider>(
            std::make_unique<metrics::DemographicsClient>(),
            metrics::MetricsLogUploader::MetricServiceType::UKM));

    RegisterUKMProviders();
  }

  if (metrics::dwa::DwaRecorder::IsDwaOrPrivateMetricsFeatureEnabled()) {
    dwa_service_ = std::make_unique<metrics::dwa::DwaService>(
        this, local_state,
        GetApplicationContext()->GetSharedURLLoaderFactory());
  }

  if (metrics::private_metrics::PumaService::IsPumaEnabled()) {
    puma_service_ = std::make_unique<metrics::private_metrics::PumaService>(
        this, local_state);
  }
}

void IOSChromeMetricsServiceClient::RegisterMetricsServiceProviders() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          base::BindRepeating(&GetNetworkConnectionTrackerAsync)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<OmniboxMetricsProvider>());

  auto stability_metrics_provider =
      std::make_unique<IOSChromeStabilityMetricsProvider>(local_state);
  stability_metrics_provider_ = stability_metrics_provider.get();
  metrics_service_->RegisterMetricsProvider(
      std::move(stability_metrics_provider));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSChromeDefaultBrowserMetricsProvider>(
          metrics::MetricsLogUploader::MetricServiceType::UMA));

  // NOTE: metrics_state_manager_->IsMetricsReportingEnabled() returns false
  // during local testing. To test locally, modify
  // MetricsServiceAccessor::IsMetricsReportingEnabled() to return true.
  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      metrics_state_manager_->IsMetricsReportingEnabled()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(local_state));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(ios::FILE_LOCAL_STATE));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSChromeSigninAndSyncStatusMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSFamilyLinkUserMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<MobileSessionShutdownMetricsProvider>(
          metrics_service_.get()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::DeviceCountMetricsProvider>(base::BindRepeating(
          &DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::PassphraseTypeMetricsProvider>(
          syncer::PassphraseTypeMetricsProvider::HistogramVersion::kV2,
          base::BindRepeating(&SyncServiceFactory::GetAllSyncServices)));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::PassphraseTypeMetricsProvider>(
          syncer::PassphraseTypeMetricsProvider::HistogramVersion::kV4,
          base::BindRepeating(&SyncServiceFactory::GetAllSyncServices)));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::PassphraseTypeMetricsProvider>(
          syncer::PassphraseTypeMetricsProvider::HistogramVersion::kV5,
          base::BindRepeating(&SyncServiceFactory::GetAllSyncServices)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<translate::TranslateRankerMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DemographicMetricsProvider>(
          std::make_unique<metrics::DemographicsClient>(),
          metrics::MetricsLogUploader::MetricServiceType::UMA));

  metrics_service_->RegisterMetricsProvider(
      CreateIOSProfileSessionMetricsProvider());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSFeedActivityMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSPushNotificationsMetricsProvider>());

  // Only register the RegionalCapabilitiesMetricsProvider if the dynamic
  // profile country feature is enabled. This is because that feature
  // significantly changes the cases under which the "Mixed" bucket is emitted.
  if (base::FeatureList::IsEnabled(switches::kDynamicProfileCountry)) {
    metrics_service_->RegisterMetricsProvider(
        std::make_unique<
            regional_capabilities::IOSRegionalCapabilitiesMetricsProvider>());
  }
}

void IOSChromeMetricsServiceClient::RegisterUKMProviders() {
  // LINT.IfChange(UkmProviders)
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::InstallDateProvider>(local_state));

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      ukm::CreateFieldTrialsProviderForUkm(synthetic_trial_registry_.get()));

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(local_state));
  // LINT.ThenChange(/chrome/browser/metrics/chrome_metrics_service_client.cc:UkmProviders)
}

void IOSChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = ios::device_util::GetTaskVMInfo();
  if (result.has_value()) {
    task_vm_info task_vm_info_data = result.value();
    mach_vm_size_t footprint_mb =
        task_vm_info_data.phys_footprint / 1024 / 1024;
    base::UmaHistogramMemoryLargeMB("Memory.Browser.MemoryFootprint",
                                    footprint_mb);
    // The pseudo metric of Memory.Browser.MemoryFootprint. Only used to
    // assess field trial data quality.
    base::UmaHistogramMemoryLargeMB(
        "UMA.Pseudo.Memory.Browser.MemoryFootprint",
        metrics::GetPseudoMetricsSample(
            static_cast<double>(task_vm_info_data.phys_footprint) / 1024 /
            1024));

    switch (UIApplication.sharedApplication.applicationState) {
      case UIApplicationStateActive:
        base::UmaHistogramMemoryLargeMB("Memory.Browser.MemoryFootprint.Active",
                                        footprint_mb);
        // According to Apple, apps on iPhone 6 and older devices get terminated
        // by the OS if memory usage crosses 200MB watermark. Obviously this
        // metric will not be recorded with true on iPhone 6 and older devices.
        UMA_HISTOGRAM_BOOLEAN(
            "Memory.Browser.MemoryFootprint.Active.Over200MBWatermark",
            footprint_mb >= 200);
        break;
      case UIApplicationStateInactive:
        base::UmaHistogramMemoryLargeMB(
            "Memory.Browser.MemoryFootprint.Inactive", footprint_mb);
        break;
      case UIApplicationStateBackground:
        base::UmaHistogramMemoryLargeMB(
            "Memory.Browser.MemoryFootprint.Background", footprint_mb);
        break;
    }
  } else {
    // Max kern_return_t is 0x100 = 256, plus trailing null.
    // (https://opensource.apple.com/source/xnu/xnu-792.25.20/osfmk/mach/kern_return.h)
    // TODO(crbug.com/40866217): Remove this when done debugging the uncaught
    // memory regression.
    static crash_reporter::CrashKeyString<4> task_info_kern_return(
        "task-info-kern-return");
    char kr_buf[4];
    base::strings::SafeSPrintf(kr_buf, "%d", result.error());
    task_info_kern_return.Set(kr_buf);
    base::debug::DumpWithoutCrashing();
  }

  int open_tabs_count = 0;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    // Iterate through regular Browser and OTR Browser to find the corresponding
    // tab.
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
    std::set<Browser*> browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kAll);

    for (Browser* browser : browsers) {
      open_tabs_count += browser->GetWebStateList()->count();
    }
  }
  base::UmaHistogramCounts10000("Memory.Browser.MemoryFootprint.NumOpenTabs",
                                open_tabs_count);

  std::move(collect_final_metrics_done_callback_).Run();
}

void IOSChromeMetricsServiceClient::RegisterForNotifications() {
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(
              &IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
              base::Unretained(this)));

  // ProfileManagerIOS invoke OnProfileLoaded(...) for all Profiles already
  // loaded, so there is no need to manually iterate over them.
  profile_manager_observation_.Observe(
      GetApplicationContext()->GetProfileManager());
}

bool IOSChromeMetricsServiceClient::RegisterForProfileEvents(
    ProfileIOS* profile) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS);
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  if (!history_service || !sync || !browser_list) {
    return false;
  }

  ObserveServiceForDeletions(history_service);
  StartObserving(sync, profile->GetPrefs());
  StartObservingBrowserList(browser_list);
  return true;
}

void IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

// static
metrics::FileMetricsProvider::FilterAction
IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles(
    const base::FilePath& path) {
  // Do not process the file if it corresponds to the current process id.
  base::ProcessId pid;
  bool parse_success = base::GlobalHistogramAllocator::ParseFilePath(
      path, nullptr, nullptr, &pid);
  if (!parse_success) {
    return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
  }
  if (pid == base::GetCurrentProcId()) {
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;
  }
  // No need to test whether `pid` is a different active process. This isn't
  // applicable to iOS because there cannot be two copies of Chrome running.
  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

metrics::EnableMetricsDefault
IOSChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      GetApplicationContext()->GetLocalState());
}

void IOSChromeMetricsServiceClient::OnHistoryDeleted() {
  if (ukm_service_) {
    ukm_service_->Purge();
  }
  if (dwa_service_) {
    dwa_service_->Purge();
  }
}

void IOSChromeMetricsServiceClient::OnUkmAllowedStateChanged(
    bool must_purge,
    ukm::UkmConsentState previous_consent_state) {
  const ukm::UkmConsentState consent_state = GetUkmConsentState();
  if (ukm_service_) {
    if (must_purge) {
      ukm_service_->Purge();
      ukm_service_->ResetClientState(
          ukm::ResetReason::kOnUkmAllowedStateChanged);
    } else {
      // Purge recording if required consent has been revoked.
      if (!consent_state.Has(ukm::MSBB)) {
        ukm_service_->PurgeMsbbData();
      }
      // No need to test for ukm::APPS and ukm::EXTENSIONS as they are not
      // supported on iOS.
    }

    // Notify the recording service of changed metrics consent.
    ukm_service_->UpdateRecording(consent_state);

    // Broadcast UKM consent state change.
    ukm_service_->OnUkmAllowedStateChanged(consent_state);
  }

  // Purges DWA data if any of the UKM consents is missing. For consent changes
  // to metrics collection, this is handled in a separate callback
  // (see MetricsServicesManager::UpdatePermissions()). Other scenarios,
  // such as incognito, are handled further downstream.
  if (dwa_service_ && (must_purge || !IsDwaAllowedForAllProfiles())) {
    dwa_service_->Purge();
  }

  // Signal service manager to enable/disable UKM/DWA based on new states.
  UpdateRunningServices();
}

void IOSChromeMetricsServiceClient::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {
  // Nothing to do, IOSChromeMetricsServiceClient does not keep any profile
  // alive.
}

void IOSChromeMetricsServiceClient::OnProfileManagerDestroyed(
    ProfileManagerIOS* manager) {
  profile_manager_observation_.Reset();
}

void IOSChromeMetricsServiceClient::OnProfileCreated(ProfileManagerIOS* manager,
                                                     ProfileIOS* profile) {
  // Nothing to do, the Profile is not fully loaded, and it is not possible to
  // access the KeyedService yet.
}

void IOSChromeMetricsServiceClient::OnProfileLoaded(ProfileManagerIOS* manager,
                                                    ProfileIOS* profile) {
  if (!RegisterForProfileEvents(profile)) {
    notification_listeners_active_ = false;
  }
}

void IOSChromeMetricsServiceClient::OnProfileUnloaded(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  // Nothing to do.
}

void IOSChromeMetricsServiceClient::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  // Nothing to do.
}

void IOSChromeMetricsServiceClient::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  switch (browser->type()) {
    case Browser::Type::kRegular:
    case Browser::Type::kIncognito:
      StartObservingBrowser(browser);
      break;

    case Browser::Type::kInactive:
    case Browser::Type::kTemporary:
      break;
  }
}

void IOSChromeMetricsServiceClient::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  switch (browser->type()) {
    case Browser::Type::kRegular:
    case Browser::Type::kIncognito:
      StopObservingBrowser(browser);
      break;

    case Browser::Type::kInactive:
    case Browser::Type::kTemporary:
      break;
  }
}

void IOSChromeMetricsServiceClient::OnBrowserListShutdown(
    BrowserList* browser_list) {
  StopObservingBrowserList(browser_list);
}

void IOSChromeMetricsServiceClient::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();

      StopObservingWebState(detach_change.detached_web_state());
      break;
    }

    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();

      StopObservingWebState(replace_change.replaced_web_state());
      StartObservingWebState(replace_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();

      StartObservingWebState(insert_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kStatusOnly:
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      // nothing to do.
      break;
  }
}

void IOSChromeMetricsServiceClient::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (stability_metrics_provider_) {
    stability_metrics_provider_->WebStateDidStartNavigation(web_state,
                                                            navigation_context);
  }
}

void IOSChromeMetricsServiceClient::DidStartLoading(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::DidStopLoading(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
  if (stability_metrics_provider_) {
    stability_metrics_provider_->WebStateDidStartLoading(web_state);
  }
}

void IOSChromeMetricsServiceClient::RenderProcessGone(
    web::WebState* web_state) {
  if (stability_metrics_provider_) {
    stability_metrics_provider_->RenderProcessGone(web_state);
  }
}

void IOSChromeMetricsServiceClient::WebStateDestroyed(
    web::WebState* web_state) {
  NOTREACHED();
}

bool IOSChromeMetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return UkmConsentStateObserver::IsUkmAllowedForAllProfiles();
}

bool IOSChromeMetricsServiceClient::IsDwaAllowedForAllProfiles() {
  return UkmConsentStateObserver::IsDwaAllowedForAllProfiles();
}

bool IOSChromeMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  return notification_listeners_active_;
}

std::string IOSChromeMetricsServiceClient::GetUploadSigningKey() {
  std::string decoded_key;
  base::Base64Decode(google_apis::GetMetricsKey(), &decoded_key);
  return decoded_key;
}

bool IOSChromeMetricsServiceClient::ShouldStartUpFast() const {
  return base::FeatureList::IsEnabled(first_run::kManualLogUploadsInTheFRE)
             ? IsFirstRun()
             : false;
}

void IOSChromeMetricsServiceClient::StartObservingBrowserList(
    BrowserList* browser_list) {
  browser_list_observations_.AddObservation(browser_list);

  BrowserList::BrowserType type = BrowserList::BrowserType::kAll;
  for (Browser* browser : browser_list->BrowsersOfType(type)) {
    OnBrowserAdded(browser_list, browser);
  }
}

void IOSChromeMetricsServiceClient::StopObservingBrowserList(
    BrowserList* browser_list) {
  browser_list_observations_.RemoveObservation(browser_list);

  BrowserList::BrowserType type = BrowserList::BrowserType::kAll;
  for (Browser* browser : browser_list->BrowsersOfType(type)) {
    OnBrowserRemoved(browser_list, browser);
  }
}

void IOSChromeMetricsServiceClient::StartObservingBrowser(Browser* browser) {
  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.AddObservation(web_state_list);

  const int web_state_list_count = web_state_list->count();
  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    StartObservingWebState(web_state);
  }
}

void IOSChromeMetricsServiceClient::StopObservingBrowser(Browser* browser) {
  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.RemoveObservation(web_state_list);

  const int web_state_list_count = web_state_list->count();
  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    StopObservingWebState(web_state);
  }
}

void IOSChromeMetricsServiceClient::StartObservingWebState(
    web::WebState* web_state) {
  web_state_observations_.AddObservation(web_state);
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::StopObservingWebState(
    web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}
