// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_main_parts.h"

#import <Foundation/Foundation.h>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/ios/ios_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/reporter_running_ios.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/expired_histogram_util.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/tribool.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "ios/chrome/browser/application_context_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_keyed_service_factories.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/flags/about_flags.h"
#include "ios/chrome/browser/install_time_util.h"
#include "ios/chrome/browser/ios_thread_profiler.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#include "ios/chrome/browser/metrics/ios_expired_histograms_array.h"
#include "ios/chrome/browser/open_from_clipboard/create_clipboard_recent_content.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/signin/signin_util.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"                        // nogncheck
#include "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"  // nogncheck
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/allocator_interception_mac.h"
#include "base/allocator/allocator_shim.h"
#endif

#if DCHECK_IS_ON()
#include "ui/display/screen_base.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sets |level| value for NSURLFileProtectionKey key for the URL with given
// |local_state_path|.
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

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
// Do not install allocator shim on iOS 13.4 due to high crash volume on this
// particular version of OS. TODO(crbug.com/1108219): Remove this workaround
// when/if the bug gets fixed.
bool ShouldInstallAllocatorShim() {
  return !base::ios::IsRunningOnOrLater(13, 4, 0) ||
         base::ios::IsRunningOnOrLater(13, 5, 0);
}
#endif

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

void IOSChromeMainParts::PreEarlyInitialization() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (ShouldInstallAllocatorShim()) {
    base::allocator::InitializeAllocatorShim();
  }
#endif
}

void IOSChromeMainParts::PreCreateMainMessageLoop() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          std::string(), nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
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
        base::ThreadTaskRunnerHandle::Get());
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
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  base::FilePath local_state_path;
  CHECK(base::PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));
  application_context_.reset(new ApplicationContextImpl(
      local_state_task_runner.get(), parsed_command_line_,
      l10n_util::GetLocaleOverride()));
  DCHECK_EQ(application_context_.get(), GetApplicationContext());

  // Check the first run state early; this must be done before IO is disallowed
  // so that later calls can use the cached value.
  static crash_reporter::CrashKeyString<4> key("first-run");
  if (FirstRun::IsChromeFirstRun())
    key.Set("yes");

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

  // Convert freeform experimental settings into switches before initializing
  // local state, in case any of the settings affect policy.
  AppendSwitchesFromExperimentalSettings(command_line);

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

  // Set metrics upload for stack/heap profiles.
  IOSThreadProfiler::SetBrowserProcessReceiverCallback(base::BindRepeating(
      &metrics::CallStackProfileMetricsProvider::ReceiveProfile));

  // Sync the crashpad field tral state to NSUserDefaults.  Called immediately
  // after setting up field trials.
  crash_helper::SyncCrashpadEnabledOnNextRun();

  // Sync the CleanExitBeacon.
  metrics::CleanExitBeacon::SyncUseUserDefaultsBeacon();

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  // Do not install allocator shim on iOS 13.4 due to high crash volume on this
  // particular version of OS. TODO(crbug.com/1108219): Remove this workaround
  // when/if the bug gets fixed.
  if (ShouldInstallAllocatorShim()) {
    bool malloc_intercepted = base::allocator::AreMallocZonesIntercepted();
    base::UmaHistogramBoolean("IOS.Allocator.ShimInstalled",
                              malloc_intercepted);

    if (malloc_intercepted) {
      // Start heap profiling as early as possible so it can start recording
      // memory allocations. Requires the allocator shim to be enabled.
      heap_profiler_controller_ = std::make_unique<HeapProfilerController>(
          channel, metrics::CallStackProfileParams::Process::kBrowser);
      heap_profiler_controller_->StartIfEnabled();
    }
  }
#endif

  variations::InitCrashKeys();

  metrics::EnableExpiryChecker(::kExpiredHistogramsHashes,
                               ::kNumExpiredHistograms);

  // TODO(crbug.com/1164533): Remove code below some time after February 2021.
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

void IOSChromeMainParts::PreMainMessageLoopRun() {
  application_context_->PreMainMessageLoopRun();

  // ContentSettingsPattern need to be initialized before creating the
  // ChromeBrowserState.
  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(nullptr, 0);

  // Ensure ClipboadRecentContentIOS is created.
  ClipboardRecentContent::SetInstance(CreateClipboardRecentContentIOS());

  // Ensure that the browser state is initialized.
  EnsureBrowserStateKeyedServiceFactoriesBuilt();
  ios::ChromeBrowserStateManager* browser_state_manager =
      application_context_->GetChromeBrowserStateManager();
  ChromeBrowserState* last_used_browser_state =
      browser_state_manager->GetLastUsedBrowserState();

  // This must occur at PreMainMessageLoopRun because |SetupMetrics()| uses the
  // blocking pool, which is disabled until the CreateThreads phase of startup.
  // TODO(crbug.com/786494): Investigate whether metrics recording can be
  // initialized consistently across iOS and non-iOS platforms
  SetupMetrics();

  // Now that the file thread has been started, start recording.
  StartMetricsRecording();

  // Because the crashpad flag takes 2 restarts to take effect, register a
  // synthetic field trial when crashpad is actually running.  Called
  // immediately after starting metrics recording.
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "CrashpadIOS",
      crash_reporter::IsCrashpadRunning() ? "Enabled" : "Disabled",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  // Because the CleanExitBeacon flag takes 2 restarts to take effect, register
  // a synthetic field trial when the user defaults beacon is set. Called
  // immediately after starting metrics recording.
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "UseUserDefaultsForExitedCleanlyBeacon",
      metrics::CleanExitBeacon::ShouldUseUserDefaultsBeacon() ? "Enabled"
                                                              : "Disabled",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

#if BUILDFLAG(ENABLE_RLZ)
  // Init the RLZ library. This just schedules a task on the file thread to be
  // run sometime later. If this is the first run we record the installation
  // event.
  int ping_delay = last_used_browser_state->GetPrefs()->GetInteger(
      FirstRun::GetPingDelayPrefName());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  rlz::RLZTracker::SetRlzDelegate(base::WrapUnique(new RLZTrackerDelegateImpl));
  rlz::RLZTracker::InitRlzDelayed(
      FirstRun::IsChromeFirstRun(), ping_delay < 0,
      base::Milliseconds(abs(ping_delay)),
      RLZTrackerDelegateImpl::IsGoogleDefaultSearch(last_used_browser_state),
      RLZTrackerDelegateImpl::IsGoogleHomepage(last_used_browser_state),
      RLZTrackerDelegateImpl::IsGoogleInStartpages(last_used_browser_state));
#endif  // BUILDFLAG(ENABLE_RLZ)

  TranslateServiceIOS::Initialize();
  language::LanguageUsageMetrics::RecordAcceptLanguages(
      last_used_browser_state->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages));
  language::LanguageUsageMetrics::RecordApplicationLanguage(
      application_context_->GetApplicationLocale());
  translate::TranslateMetricsLoggerImpl::LogApplicationStartMetrics(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          last_used_browser_state->GetPrefs()));

  // Request new variations seed information from server.
  variations::VariationsService* variations_service =
      application_context_->GetVariationsService();
  if (variations_service) {
    variations_service->set_policy_pref_service(
        last_used_browser_state->GetPrefs());
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
  base::FilePath user_data_path;
  CHECK(base::PathService::Get(ios::DIR_USER_DATA, &user_data_path));
  safe_browsing::SafeBrowsingMetricsCollector* safe_browsing_metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForBrowserState(
          last_used_browser_state);
  safe_browsing_service->Initialize(last_used_browser_state->GetPrefs(),
                                    user_data_path,
                                    safe_browsing_metrics_collector);

  // Set monitoring for some experimental flags.
  MonitorExperimentalSettingsChanges();

  // Ensure the Fullscren Promos Manager is initialized.
  PromosManager* promos_manager = application_context_->GetPromosManager();
  promos_manager->Init();
}

void IOSChromeMainParts::PostMainMessageLoopRun() {
  TranslateServiceIOS::Shutdown();
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
  variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);

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

  application_context_->GetVariationsService()->SetUpFieldTrials(
      variation_ids, command_line_variation_ids,
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::move(feature_list), &ios_field_trials_);
}

void IOSChromeMainParts::SetupMetrics() {
  metrics::MetricsService* metrics = application_context_->GetMetricsService();
  metrics->GetSyntheticTrialRegistry()->AddSyntheticTrialObserver(
      variations::VariationsIdsProvider::GetInstance());
  metrics->GetSyntheticTrialRegistry()->AddSyntheticTrialObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());
  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();
}

void IOSChromeMainParts::StartMetricsRecording() {
  application_context_->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
}
