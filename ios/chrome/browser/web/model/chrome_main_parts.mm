// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/chrome_main_parts.h"

#import <Foundation/Foundation.h>

#import "base/allocator/partition_alloc_support.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/features.h"
#import "base/files/file_path.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/time/default_tick_clock.h"
#import "build/blink_buildflags.h"
#import "components/content_settings/core/browser/cookie_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/flags_ui/pref_service_flags_storage.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language/core/browser/language_usage_metrics.h"
#import "components/language/core/browser/pref_names.h"
#import "components/memory_system/initializer.h"
#import "components/memory_system/parameters.h"
#import "components/metrics/call_stacks/call_stack_profile_builder.h"
#import "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#import "components/metrics/clean_exit_beacon.h"
#import "components/metrics/expired_histogram_util.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/sampling_profiler/process_type.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/browser/translate_metrics_logger_impl.h"
#import "components/variations/field_trial_config/field_trial_util.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/synthetic_trial_registry.h"
#import "components/variations/synthetic_trials_active_group_id_provider.h"
#import "components/variations/variations_crash_keys.h"
#import "components/variations/variations_ids_provider.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/application_context/model/application_context_impl.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/flags/about_flags.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/metrics/model/ios_expired_histograms_array.h"
#import "ios/chrome/browser/open_from_clipboard/model/create_clipboard_recent_content.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/profile/model/keyed_service_factories.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/ukm_database_client.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
#import "ios/chrome/browser/web/model/ios_thread_profiler.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/public/provider/chrome/browser/additional_features/additional_features_controller.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/network_change_notifier.h"
#import "net/http/http_network_layer.h"
#import "net/http/http_stream_factory.h"
#import "net/url_request/url_request.h"
#import "rlz/buildflags/buildflags.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_RLZ)
#import "components/rlz/rlz_tracker.h"                        // nogncheck
#import "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"  // nogncheck
#endif

#if DCHECK_IS_ON()
#import "ui/display/screen_base.h"
#endif

namespace {

// Sets `level` value for NSURLFileProtectionKey key for the URL with given
// `local_state_path`.
void SetProtectionLevel(const base::FilePath& file_path, id level) {
  NSString* file_path_string = base::SysUTF8ToNSString(file_path.value());
  NSURL* file_path_url = [NSURL fileURLWithPath:file_path_string
                                    isDirectory:NO];
  NSError* error = nil;
  BOOL protection_set = [file_path_url setResourceValue:level
                                                 forKey:NSURLFileProtectionKey
                                                  error:&error];
  DCHECK(protection_set) << base::SysNSStringToUTF8(error.localizedDescription);
}

}  // namespace

IOSChromeMainParts::IOSChromeMainParts(
    const base::CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line), local_state_(nullptr) {
  // Chrome disallows cookies by default. All code paths that want to use
  // cookies need to go through one of Chrome's URLRequestContexts which have
  // a ChromeNetworkDelegate attached that selectively allows cookies again.
  net::URLRequest::SetDefaultCookiePolicyToBlock();
}

IOSChromeMainParts::~IOSChromeMainParts() {
#if DCHECK_IS_ON()
  // The screen object is never deleted on IOS. Make sure that all display
  // observers are removed at the end.
  display::ScreenBase* screen =
      static_cast<display::ScreenBase*>(display::Screen::GetScreen());
  DCHECK(!screen->HasDisplayObservers());
#endif
}

void IOSChromeMainParts::PreCreateMainMessageLoop() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          std::string(), nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  std::string app_locale = l10n_util::GetApplicationLocale(std::string());
  [[PreviousSessionInfo sharedInstance]
      setReportParameterValue:base::SysUTF8ToNSString(app_locale)
                       forKey:@"icu_locale_input"];
  CHECK(!loaded_locale.empty());

  base::FilePath resources_pack_path;
  base::PathService::Get(ios::FILE_RESOURCES_PACK, &resources_pack_path);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::k100Percent);
}

void IOSChromeMainParts::PreCreateThreads() {
  // Create and start the stack sampling profiler if CANARY or DEV. The warning
  // below doesn't apply.
  const version_info::Channel channel = ::GetChannel();
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV) {
    sampling_profiler_ = IOSThreadProfiler::CreateAndStartOnMainThread();
    IOSThreadProfiler::SetMainThreadTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  // IMPORTANT
  // Calls in this function should not post tasks or create threads as
  // components used to handle those tasks are not yet available. This work
  // should be deferred to PreMainMessageLoopRunImpl.

  // The initial read is done synchronously, the TaskPriority is thus only used
  // for flushes to disks and BACKGROUND is therefore appropriate. Priority of
  // remaining BACKGROUND+BLOCK_SHUTDOWN tasks is bumped by the ThreadPool on
  // shutdown.
  scoped_refptr<base::SequencedTaskRunner> local_state_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  base::FilePath local_state_path;
  CHECK(base::PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));
  application_context_.reset(new ApplicationContextImpl(
      local_state_task_runner.get(), *parsed_command_line_,
      l10n_util::GetLocaleOverride(),
      base::SysNSStringToUTF8(
          [[NSLocale currentLocale] objectForKey:NSLocaleCountryCode])));
  DCHECK_EQ(application_context_.get(), GetApplicationContext());

  // Check the first run state early; this must be done before IO is disallowed
  // so that later calls can use the cached value.
  static crash_reporter::CrashKeyString<4> key("first-run");
  if (FirstRun::IsChromeFirstRun()) {
    key.Set("yes");
  }

  // Compute device restore flag before IO is disallowed on UI thread, so the
  // value is available from cache synchronously.
  static crash_reporter::CrashKeyString<8> device_restore_key("device-restore");
  switch (IsFirstSessionAfterDeviceRestore()) {
    case signin::Tribool::kTrue:
      device_restore_key.Set("yes");
      break;
    case signin::Tribool::kFalse:
      break;
    case signin::Tribool::kUnknown:
      device_restore_key.Set("unknown");
      break;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Convert freeform experimental settings into switches before initializing
  // local state, in case any of the settings affect policy.
  AppendSwitchesFromExperimentalSettings(command_line);

  // Get the variation IDs passed through the command line. This is done early
  // on because ConvertFlagsToSwitches() will append to the command line
  // the variation IDs from flags (so that they are visible in about://version).
  // This will be passed on to `VariationsService::SetUpFieldTrials()`, which
  // will manually fetch the variation IDs from flags (hence the reason we do
  // not pass the mutated command line, otherwise the IDs will be duplicated).
  // It also distinguishes between variation IDs coming from the command line
  // and from flags, so we cannot rely on simply putting them all in the
  // command line.
  const std::string command_line_variation_ids =
      command_line->GetSwitchValueASCII(
          variations::switches::kForceVariationIds);

  // Initialize local state.
  local_state_ = application_context_->GetLocalState();
  DCHECK(local_state_);

  flags_ui::PrefServiceFlagsStorage flags_storage(
      application_context_->GetLocalState());
  ConvertFlagsToSwitches(&flags_storage, command_line);

  // Now that the command line has been mutated based on about:flags, we can
  // initialize field trials. The field trials are needed by IOThread's
  // initialization which happens in BrowserProcess:PreCreateThreads. Metrics
  // initialization is handled in PreMainMessageLoopRun since it posts tasks.
  SetUpFieldTrials(command_line_variation_ids);

  // Initialize //base features that depend on the `FeatureList`. Don't force
  // emitting profiler metadata since the profiler doesn't run on iOS.
  base::features::Init(
      base::features::EmitThreadControllerProfilerMetadata::kFeatureDependent);

  // Set metrics upload for stack/heap profiles.
  IOSThreadProfiler::SetBrowserProcessReceiverCallback(base::BindRepeating(
      &metrics::CallStackProfileMetricsProvider::ReceiveProfile));

  // Sync the CleanExitBeacon.
  metrics::CleanExitBeacon::SyncUseUserDefaultsBeacon();

  // On iOS we know that ProfilingClient is the only user of
  // PoissonAllocationSampler, there are no others. Therefore, make
  // memory_system include it dynamically.
  // We pass an empty string as process type to the dispatcher to keep
  // consistency with other main delegates where the browser process is denoted
  // this way.
  memory_system::Initializer()
      .SetProfilingClientParameters(
          channel, sampling_profiler::ProfilerProcessType::kBrowser)
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kDynamic,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore,
                               "")
      .Initialize(memory_system_);

  variations::InitCrashKeys();

  metrics::EnableExpiryChecker(::kExpiredHistogramsHashes);

  // TODO(crbug.com/40163579): Remove code below some time after February 2021.
  NSString* const kRemoveProtectionFromPrefFileKey =
      @"RemoveProtectionFromPrefKey";
  if ([NSUserDefaults.standardUserDefaults
          boolForKey:kRemoveProtectionFromPrefFileKey]) {
    // Restore default protection level when user is no longer in the
    // experimental group.
    SetProtectionLevel(local_state_path,
                       NSFileProtectionCompleteUntilFirstUserAuthentication);
    [NSUserDefaults.standardUserDefaults
        removeObjectForKey:kRemoveProtectionFromPrefFileKey];
  }

  application_context_->PreCreateThreads();
}

void IOSChromeMainParts::PostCreateThreads() {
  application_context_->PostCreateThreads();
}

void IOSChromeMainParts::PreMainMessageLoopRun() {
  application_context_->PreMainMessageLoopRun();

  // Retrieve first run information for future use.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FirstRun::LoadSentinelInfo));

  // ContentSettingsPattern need to be initialized before creating the
  // ProfileIOS.
  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(nullptr, 0);

  // Ensure ClipboadRecentContentIOS is created.
  ClipboardRecentContent::SetInstance(CreateClipboardRecentContentIOS());

  // Initialize opt guide.
  OptimizationGuideServiceFactory::InitializePredictionModelStore();

  segmentation_platform::UkmDatabaseClientHolder::GetClientInstance(nullptr)
      .PreProfileInit(
          /*in_memory_database=*/false);

  // Ensure that the KeyedService factories are registered.
  EnsureProfileKeyedServiceFactoriesBuilt();
  BrowserStateDependencyManager::GetInstance()
      ->DisallowKeyedServiceFactoryRegistration(
          "EnsureProfileKeyedServiceFactoriesBuilt()");

  // Ensure the Profiles are loaded and initialized.
  ProfileManagerIOS* profile_manager =
      application_context_->GetProfileManager();

  // Load all Profiles.
  profile_manager->LoadProfiles();

  // TODO(crbug.com/325257407): Factor all of the code that uses this to instead
  // initialize for every profile.
  std::vector<ProfileIOS*> profiles = profile_manager->GetLoadedProfiles();
  ProfileIOS* last_used_profile = profiles.at(0);

  // This must occur at PreMainMessageLoopRun because `SetupMetrics()` uses the
  // blocking pool, which is disabled until the CreateThreads phase of startup.
  // TODO(crbug.com/41356264): Investigate whether metrics recording can be
  // initialized consistently across iOS and non-iOS platforms
  SetupMetrics();

  // Now that the file thread has been started, start recording.
  StartMetricsRecording();

  // Because the CleanExitBeacon flag takes 2 restarts to take effect, register
  // a synthetic field trial when the user defaults beacon is set. Called
  // immediately after starting metrics recording.
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "UseUserDefaultsForExitedCleanlyBeacon",
      metrics::CleanExitBeacon::ShouldUseUserDefaultsBeacon() ? "Enabled"
                                                              : "Disabled",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  segmentation_platform::UkmDatabaseClientHolder::GetClientInstance(nullptr)
      .StartObservation();

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  base::allocator::PartitionAllocSupport::Get()
      ->ReconfigureAfterFeatureListInit("");
  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      "");
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

#if BUILDFLAG(ENABLE_RLZ)
  // Init the RLZ library. This just schedules a task on the file thread to be
  // run sometime later. If this is the first run we record the installation
  // event.
  int ping_delay = last_used_profile->GetPrefs()->GetInteger(
      FirstRun::GetPingDelayPrefName());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  rlz::RLZTracker::SetRlzDelegate(base::WrapUnique(new RLZTrackerDelegateImpl));
  rlz::RLZTracker::InitRlzDelayed(
      FirstRun::IsChromeFirstRun(), ping_delay < 0,
      base::Milliseconds(abs(ping_delay)),
      RLZTrackerDelegateImpl::IsGoogleDefaultSearch(last_used_profile),
      RLZTrackerDelegateImpl::IsGoogleHomepage(last_used_profile),
      RLZTrackerDelegateImpl::IsGoogleInStartpages(last_used_profile));
#endif  // BUILDFLAG(ENABLE_RLZ)

  TranslateServiceIOS::Initialize();
  language::LanguageUsageMetrics::RecordAcceptLanguages(
      last_used_profile->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages));
  translate::TranslateMetricsLoggerImpl::LogApplicationStartMetrics(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          last_used_profile->GetPrefs()));

  // Request new variations seed information from server.
  variations::VariationsService* variations_service =
      application_context_->GetVariationsService();
  if (variations_service) {
    variations_service->set_policy_pref_service(last_used_profile->GetPrefs());
    variations_service->PerformPreMainMessageLoopStartup();
  }

  // Initialize Chrome Browser Cloud Management.
  auto* policy_connector = application_context_->GetBrowserPolicyConnector();
  if (policy_connector) {
    policy_connector->chrome_browser_cloud_management_controller()->Init(
        application_context_->GetLocalState(),
        application_context_->GetSharedURLLoaderFactory());
  }

  // Ensure that Safe Browsing is initialized.
  SafeBrowsingService* safe_browsing_service =
      application_context_->GetSafeBrowsingService();
  safe_browsing::SafeBrowsingMetricsCollector* safe_browsing_metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(last_used_profile);
  safe_browsing_service->Initialize(
      last_used_profile->GetPrefs(),
      base::PathService::CheckedGet(ios::DIR_USER_DATA),
      safe_browsing_metrics_collector);
}

void IOSChromeMainParts::PostMainMessageLoopRun() {
  TranslateServiceIOS::Shutdown();

  segmentation_platform::UkmDatabaseClientHolder::GetClientInstance(nullptr)
      .PostMessageLoopRun();

#if BUILDFLAG(ENABLE_RLZ)
  rlz::RLZTracker::CleanupRlz();
#endif  // BUILDFLAG(ENABLE_RLZ)
  application_context_->StartTearDown();
}

void IOSChromeMainParts::PostDestroyThreads() {
  application_context_->PostDestroyThreads();
}

// This will be called after the command-line has been mutated by about:flags
void IOSChromeMainParts::SetUpFieldTrials(
    const std::string& command_line_variation_ids) {
  base::SetRecordActionTaskRunner(web::GetUIThreadTaskRunner({}));

  // FeatureList requires VariationsIdsProvider to be created.
#if !BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/40261735) Move variations to PostEarlyInitialization.
  variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);
#endif

  // Initialize FieldTrialList to support FieldTrials that use one-time
  // randomization.
  application_context_->GetMetricsServicesManager()
      ->InstantiateFieldTrialList();

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

  // Associate parameters chosen in about:flags and create trial/group for them.
  flags_ui::PrefServiceFlagsStorage flags_storage(
      application_context_->GetLocalState());
  std::vector<std::string> variation_ids =
      RegisterAllFeatureVariationParameters(&flags_storage, feature_list.get());

  // Register additional features to the feature list.
  AdditionalFeaturesController* additional_features_controller =
      application_context_->GetAdditionalFeaturesController();
  additional_features_controller->RegisterFeatureList(feature_list.get());

#if !BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/40261735) Move variations to PostEarlyInitialization.
  application_context_->GetVariationsService()->SetUpFieldTrials(
      variation_ids, command_line_variation_ids,
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::move(feature_list), &ios_field_trials_);
  additional_features_controller->FeatureListDidCompleteSetup();
#endif
}

void IOSChromeMainParts::SetupMetrics() {
  metrics::MetricsService* metrics = application_context_->GetMetricsService();
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::VariationsIdsProvider::GetInstance());
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());
  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();
}

void IOSChromeMainParts::StartMetricsRecording() {
  // TODO(crbug.com/40894426) Add an EG2 test for cloned install detection.
  application_context_->GetMetricsService()->CheckForClonedInstall();
  application_context_->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
}
