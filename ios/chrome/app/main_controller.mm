// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#import <objc/runtime.h>

#import <memory>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/barrier_closure.h"
#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/functional/concurrent_closures.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/installer_policies/autofill_states_component_installer.h"
#import "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#import "components/component_updater/installer_policies/optimization_hints_component_installer.h"
#import "components/component_updater/installer_policies/plus_address_blocklist_component_installer.h"
#import "components/component_updater/installer_policies/safety_tips_component_installer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/passwords_directory_util_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/sync/service/sync_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/app_metrics_app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"
#import "ios/chrome/app/background_refresh/test_refresher.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/deferred_initialization_task_names.h"
#import "ios/chrome/app/enterprise_app_agent.h"
#import "ios/chrome/app/fast_app_terminate_buildflags.h"
#import "ios/chrome/app/launch_screen_view_controller.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/profile/profile_controller.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/safe_mode_app_state_agent.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/app/startup/chrome_main_starter.h"
#import "ios/chrome/app/startup/client_registration.h"
#import "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/chrome/app/startup/ios_enable_sandbox_dump_buildflags.h"
#import "ios/chrome/app/startup/provider_registration.h"
#import "ios/chrome/app/startup/register_experimental_settings.h"
#import "ios/chrome/app/startup/setup_debugging.h"
#import "ios/chrome/app/startup_tasks.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/app/variations_app_state_agent.h"
#import "ios/chrome/browser/accessibility/model/window_accessibility_change_notifier_app_agent.h"
#import "ios/chrome/browser/appearance/ui_bundled/appearance_customization.h"
#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/memory/model/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/model/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/model/incognito_usage_app_state_agent.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/metrics/model/window_configuration_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omaha/model/omaha_service.h"
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/screenshot/model/screenshot_metrics_recorder.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/ui_bundled/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_field_trial_version.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/empty_nsurlcache.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/public/provider/chrome/browser/memory_experimenter/memory_experimenter_api.h"
#import "ios/public/provider/chrome/browser/overrides/overrides_api.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/app/credential_provider_migrator_app_agent.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_service_factory.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_support.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#endif

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#import "ios/chrome/app/dump_documents_statistics.h"
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

namespace {

#if BUILDFLAG(FAST_APP_TERMINATE_ENABLED)
// Skip chromeMain.reset() on shutdown, see crbug.com/1328891 for details.
BASE_FEATURE(kFastApplicationWillTerminate,
             "FastApplicationWillTerminate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(FAST_APP_TERMINATE_ENABLED)

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

// Constant for deferring the cleanup of discarded sessions on disk.
NSString* const kCleanupDiscardedSessions = @"CleanupDiscardedSessions";

// Constants for deferring saving field trial values
NSString* const kSaveFieldTrialValues = @"SaveFieldTrialValues";

// Constants for refreshing the WidgetKit after five minutes
NSString* const kWidgetKitRefreshFiveMinutes = @"WidgetKitRefreshFiveMinutes";

// Constants for deferred check if it is necessary to send pings to
// Chrome distribution related services.
NSString* const kSendInstallPingIfNecessary = @"SendInstallPingIfNecessary";

// Constants for deferred deletion of leftover user downloaded files.
NSString* const kDeleteDownloads = @"DeleteDownloads";

// Constants for deferred deletion of leftover user chosen files for upload.
NSString* const kDeleteChooseFile = @"DeleteChooseFile";

// Constants for deferred deletion of leftover temporary passwords files.
NSString* const kDeleteTempPasswords = @"DeleteTempPasswords";

// Constants for deferred UMA logging of existing Siri User shortcuts.
NSString* const kLogSiriShortcuts = @"LogSiriShortcuts";

// Constants for deferred sending of queued feedback.
NSString* const kSendQueuedFeedback = @"SendQueuedFeedback";

// Constants for deferring the upload of crash reports.
NSString* const kUploadCrashReports = @"UploadCrashReports";

// Constants for deferring startup Spotlight bookmark indexing.
NSString* const kStartSpotlightBookmarksIndexing =
    @"StartSpotlightBookmarksIndexing";

// Constants for deferring the enterprise managed device check.
NSString* const kEnterpriseManagedDeviceCheck = @"EnterpriseManagedDeviceCheck";

// Constants for deferred deletion of leftover session state files.
NSString* const kPurgeWebSessionStates = @"PurgeWebSessionStates";

// Constant for deffered memory experimentation.
NSString* const kMemoryExperimentation = @"BeginMemoryExperimentation";

// Constants for deferred initilization of reading list download service.
NSString* const kInitializeReadingListDownloadService =
    @"InitializeReadingListDownloadService";

// Adapted from chrome/browser/ui/browser_init.cc.
void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      GetApplicationContext()->GetComponentUpdateService();
  DCHECK(cus);
  RegisterOnDeviceHeadSuggestComponent(
      cus, GetApplicationContext()->GetApplicationLocale());
  RegisterSafetyTipsComponent(cus);
  RegisterAutofillStatesComponent(cus,
                                  GetApplicationContext()->GetLocalState());
  RegisterOptimizationHintsComponent(cus);
  RegisterPlusAddressBlocklistComponent(cus);
}

// The delay before beginning memory experimentation.
constexpr base::TimeDelta kMemoryExperimentationDelay = base::Minutes(1);

// Schedules memory experimentation.
void BeginMemoryExperimentationAfterDelay() {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               kMemoryExperimentationDelay.InNanoseconds()),
                 dispatch_get_main_queue(), ^{
                   ios::provider::BeginMemoryExperimentation();
                 });
}

}  // namespace

// Helper class allowing to wait for the ProfileState to reach a specific
// initialisation stage.
@interface ChangeProfileObserver : NSObject <ProfileStateObserver>

+ (void)waitForProfile:(ProfileState*)profileState
      toReachInitStage:(ProfileInitStage)initStage
            completion:(ChangeProfileCompletion)completion;

@end

@interface ChangeProfileObserver ()

- (void)waitForProfile:(ProfileState*)profileState
      toReachInitStage:(ProfileInitStage)initStage
            completion:(ChangeProfileCompletion)completion;

@end

@implementation ChangeProfileObserver {
  ChangeProfileCompletion _completion;
  ProfileInitStage _initStage;
}

+ (void)waitForProfile:(ProfileState*)profileState
      toReachInitStage:(ProfileInitStage)initStage
            completion:(ChangeProfileCompletion)completion {
  // If the init stage is already reached, skip the creation of the
  // ChangeProfileObserver (as it would be immediately destroyed).
  if (profileState.initStage >= initStage) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion, /*success=*/true));
    return;
  }

  // The ChangeProfileObserver instance attaches itself as an associated object
  // of the ProfileState, taking care of destroying itself when the init stage
  // is reached. There is no need to retain it here.
  ChangeProfileObserver* observer = [[self alloc] init];
  [observer waitForProfile:profileState
          toReachInitStage:initStage
                completion:completion];
}

- (void)waitForProfile:(ProfileState*)profileState
      toReachInitStage:(ProfileInitStage)initStage
            completion:(ChangeProfileCompletion)completion {
  DCHECK_LT(profileState.initStage, initStage);
  _completion = completion;
  _initStage = initStage;

  // Ensure the object lifetime is tied to that of ProfileState.
  objc_setAssociatedObject(profileState, [self associationKey], self,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  [profileState addObserver:self];
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == _initStage) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(_completion, /*success=*/true));

    // Stop observing the ProfileState and detach self. This will cause
    // the object to be deallocated, thus nothing should happen after
    // this line.
    [profileState removeObserver:self];
    objc_setAssociatedObject(profileState, [self associationKey], nil,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
}

- (void*)associationKey {
  return &_initStage;
}

@end

@interface MainController () <AppStateObserver,
                              BlockingSceneCommands,
                              ChangeProfileCommands,
                              PrefObserverDelegate,
                              ProfileStateObserver,
                              SceneStateObserver>

// Handles collecting metrics on user triggered screenshots
@property(nonatomic, strong)
    ScreenshotMetricsRecorder* screenshotMetricsRecorder;
// Cleanup discarded sessions on disk.
- (void)cleanupDiscardedSessions;
// Pings distribution services.
- (void)pingDistributionServices;
// Sends any feedback that happens to still be on local storage.
- (void)sendQueuedFeedback;
// Called whenever an orientation change is received.
- (void)orientationDidChange:(NSNotification*)notification;
// Register to receive orientation change notification to update crash keys.
- (void)registerForOrientationChangeNotifications;
// Asynchronously creates the pref observers.
- (void)schedulePrefObserverInitialization;
// Asynchronously schedules pings to distribution services.
- (void)scheduleAppDistributionPings;
// Asynchronously schedule the init of the memoryDebuggerManager.
- (void)scheduleMemoryDebuggingTools;
// Asynchronously kick off regular free memory checks.
- (void)startFreeMemoryMonitoring;
// Asynchronously schedules the reset of the failed startup attempt counter.
- (void)scheduleStartupAttemptReset;
// Asynchronously schedules the upload of crash reports.
- (void)scheduleCrashReportUpload;
// Asynchronously schedules the cleanup of discarded session files on disk.
- (void)scheduleDiscardedSessionsCleanup;
// Schedules various tasks to be performed after the application becomes active.
- (void)scheduleLowPriorityStartupTasks;
// Schedules the deletion of user downloaded files that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteTempDownloadsDirectory;
// Schedules the deletion of file user chosed to upload that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteTempChooseFileDirectory;
// Schedule the deletion of the temporary passwords files that might
// be left over from incomplete export operations.
- (void)scheduleDeleteTempPasswordsDirectory;
// Schedule the start of memory experimentation.
- (void)scheduleMemoryExperimentation;
// Crashes the application if requested.
- (void)crashIfRequested;
// Performs synchronous profile initialization steps.
- (void)initializeProfile:(ProfileIOS*)profile;
// Initializes the application to the minimum initialization needed in all
// cases.
- (void)startUpBrowserBasicInitialization;
// Initializes the browser objects for the background handlers, perform any
// background initilisation that are required, and then transition to the
// next stage.
- (void)startUpBrowserBackgroundInitialization;
// Performs any initialisation that are required before the ProfileIOS can
// be used (mostly migrating the session storage) and asynchronously progress
// the initialisation stage when done.
- (void)startUpBrowserBackgroundProfilesInitialization;
// Initializes the browser objects for the browser UI (e.g., the browser
// state).
- (void)startUpBrowserForegroundInitialization;

@end

@implementation MainController {
  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;

  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // True if the launch metrics have already been recorded.
  BOOL _launchMetricsRecorded;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _localStatePrefObserverBridge;

  // Registrar for pref changes notifications to the local state.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // The class in charge of showing/hiding the memory debugger when the
  // appropriate pref changes.
  MemoryDebuggerManager* _memoryDebuggerManager;

  // Responsible for indexing chrome links (such as bookmarks, most likely...)
  // in system Spotlight index for all loaded profiles.
  NSMutableArray<SpotlightManager*>* _spotlightManagers;

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  // Holds the ProfileController for all loaded profiles.
  std::map<std::string, ProfileController*> _profileControllers;

  WindowConfigurationRecorder* _windowConfigurationRecorder;

  // Handler for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;

  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

// Defined by public protocols.
// - StartupInformation
@synthesize isColdStart = _isColdStart;
@synthesize appLaunchTime = _appLaunchTime;
@synthesize isFirstRun = _isFirstRun;
@synthesize didFinishLaunchingTime = _didFinishLaunchingTime;
@synthesize firstSceneConnectionTime = _firstSceneConnectionTime;

SEQUENCE_CHECKER(_sequenceChecker);

#pragma mark - Application lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _isFirstRun = ShouldPresentFirstRunExperience();
    _startupTasks = [[StartupTasks alloc] init];
    _spotlightManagers = [NSMutableArray array];
  }
  return self;
}

- (void)dealloc {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)startUpBrowserBasicInitialization {
  _appLaunchTime = IOSChromeMain::StartTime();
  _isColdStart = YES;
  UMA_HISTOGRAM_BOOLEAN("IOS.Process.ActivePrewarm",
                        base::ios::IsApplicationPreWarmed());

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];

  // Start dispatching for blocking UI commands.
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BlockingSceneCommands)];

  // Start dispatching for profile change commands.
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(ChangeProfileCommands)];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(self.appState.initStage > AppInitStage::kSafeMode);

  NSBundle* baseBundle = base::apple::OuterBundle();
  base::apple::SetBaseBundleID(
      base::SysNSStringToUTF8([baseBundle bundleIdentifier]).c_str());

  // Register default values for experimental settings (Application Preferences)
  // and set the "Version" key in the UserDefaults.
  [RegisterExperimentalSettings
      registerExperimentalSettingsWithUserDefaults:[NSUserDefaults
                                                       standardUserDefaults]
                                            bundle:base::apple::
                                                       FrameworkBundle()];

  // Register all clients before calling any web code.
  [ClientRegistration registerClients];

  _chromeMain = [ChromeMainStarter startChromeMain];

  // Start recording field trial info.
  [[PreviousSessionInfo sharedInstance] beginRecordingFieldTrials];

  // Give tests a chance to prepare for testing.
  tests_hook::SetUpTestsIfPresent();

  // Initialize the provider UI global state.
  ios::provider::InitializeUI();

  // If the user has interacted with the app, then start (or continue) watching
  // for crashes. Otherwise, do not watch for crashes.
  //
  // Depending on the client's ExtendedVariationsSafeMode experiment group (see
  // MaybeExtendVariationsSafeMode() in variations_field_trial_creator.cc for
  // more info), the signal to start watching for crashes may have occurred
  // earlier.
  //
  // TODO(b/184937096): Remove the below call to OnAppEnterForeground() if this
  // call is moved earlier for all clients. It is is being kept here for the
  // time being for the control group of the extended Variations Safe Mode
  // experiment.
  //
  // TODO(crbug.com/40190949): Stop watching for a crash if this is a background
  // fetch.
  if (_appState.userInteracted) {
    GetApplicationContext()->GetMetricsService()->OnAppEnterForeground();
  }

  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());

  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
  [self.appState queueTransitionToNextInitStage];
}

- (void)startUpBrowserBackgroundProfilesInitialization {
  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  // Should transition the init stage when all ProfileIOS have been migrated.
  __weak __typeof(self) weakSelf = self;
  base::RepeatingClosure closure =
      base::BarrierClosure(loadedProfiles.size(), base::BindOnce(^{
                             [weakSelf.appState queueTransitionToNextInitStage];
                           }));

  // MigrateSessionStorageFormat is synchronous if the storage is already in
  // the requested format, so this is safe to call and won't block the app
  // startup.
  for (ProfileIOS* profile : loadedProfiles) {
    SessionRestorationServiceFactory::GetInstance()
        ->MigrateSessionStorageFormat(
            profile, SessionRestorationServiceFactory::kOptimized, closure);
  }
}

// This initialization must happen before any windows are created.
- (void)startUpBeforeFirstWindowCreated {
  GetApplicationContext()->OnAppEnterForeground();

  // Although this duplicates some metrics_service startup logic also in
  // IOSChromeMain(), this call does additional work, checking for wifi-only
  // and setting up the required support structures.
  [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Crash the app during startup if requested but only after we have enabled
  // uploading crash reports.
  [self crashIfRequested];

  if (experimental_flags::MustClearApplicationGroupSandbox()) {
    // Clear the Application group sandbox if requested. This operation take
    // some time and will access the file system synchronously as the rest of
    // the startup sequence requires it to be completed before continuing.
    app_group::ClearAppGroupSandbox();
  }

  RegisterComponentsForUpdate();

  [[PreviousSessionInfo sharedInstance] resetConnectedSceneSessionIDs];

  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile);
    // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
    // start.
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);

    [_metricsMediator notifyCredentialProviderWasUsed:tracker];

    [_spotlightManagers
        addObject:[SpotlightManager spotlightManagerWithProfile:profile]];

    ShareExtensionService* service =
        ShareExtensionServiceFactory::GetForProfile(profile);
    service->Initialize();

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
    if (IsCredentialProviderExtensionSupported()) {
      CredentialProviderServiceFactory::GetForProfile(profile);
    }
#endif
  }

  _windowConfigurationRecorder = [[WindowConfigurationRecorder alloc] init];
}

// This initialization must only happen once there's at least one Chrome window
// open.
- (void)startUpAfterFirstWindowCreated {
  // "Low priority" tasks
  [_startupTasks registerForApplicationWillResignActiveNotification];
  [self registerForOrientationChangeNotifications];

  CustomizeUIAppearance();

  // Schedule the prefs observer init first to ensure kMetricsReportingEnabled
  // is synced before starting uploads.
  [self schedulePrefObserverInitialization];
  [self scheduleCrashReportUpload];

  // Remove all discarded sessions from disk.
  [self scheduleDiscardedSessionsCleanup];

  ios::provider::InstallOverrides();

  [self scheduleLowPriorityStartupTasks];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();

  self.screenshotMetricsRecorder = [[ScreenshotMetricsRecorder alloc] init];
  [self.screenshotMetricsRecorder startRecordingMetrics];
}

- (PostCrashAction)postCrashAction {
  if (self.appState.resumingFromSafeMode)
    return PostCrashAction::kShowSafeMode;

  if (GetApplicationContext()->WasLastShutdownClean())
    return PostCrashAction::kRestoreTabsCleanShutdown;

  if (crash_util::GetFailedStartupAttemptCount() >= 2) {
    return PostCrashAction::kShowNTPWithReturnToTab;
  }

  return PostCrashAction::kRestoreTabsUncleanShutdown;
}

- (void)startUpBrowserForegroundInitialization {
  // TODO(crbug.com/40190949): Determine whether Chrome needs to resume watching
  // for crashes.

  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  for (ProfileIOS* profile : loadedProfiles) {
    [self initializeProfile:profile];
  }
  DCHECK(!_profileControllers.empty());

  for (SceneState* sceneState in self.appState.connectedScenes) {
    [self attachProfileToSceneState:sceneState];
  }

  self.appState.postCrashAction = [self postCrashAction];
  [self startUpBeforeFirstWindowCreated];
  base::UmaHistogramEnumeration("Stability.IOS.PostCrashAction",
                                self.appState.postCrashAction);
}

- (void)initializeProfile:(ProfileIOS*)profile {
  DCHECK(!profile->IsOffTheRecord());

  ProfileController* controller =
      [[ProfileController alloc] initWithAppState:self.appState];
  [controller.state addObserver:self];
  controller.state.profile = profile;
  auto insertion_result = _profileControllers.insert(
      std::make_pair(profile->GetProfileName(), controller));
  DCHECK(insertion_result.second);

  search_engines::UpdateSearchEngineCountryCodeIfNeeded(profile->GetPrefs());

  // Force desktop mode when raccoon is enabled.
  if (ios::provider::IsRaccoonEnabled()) {
    if (!profile->GetPrefs()->GetBoolean(prefs::kUserAgentWasChanged)) {
      HostContentSettingsMap* settingsMap =
          ios::HostContentSettingsMapFactory::GetForProfile(profile);
      settingsMap->SetDefaultContentSetting(
          ContentSettingsType::REQUEST_DESKTOP_SITE, CONTENT_SETTING_ALLOW);
      profile->GetPrefs()->SetBoolean(prefs::kUserAgentWasChanged, true);
    }
  }

  if (IsTabGroupSyncEnabled()) {
    // Ensure that the tab group sync services are created to observe updates.
    tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  }

  // Stop forcing the orientation at the application level. ProfileController
  // take care of forcing the orientation of the application until done with
  // the early UI initialisation.
  _scopedForceOrientation.reset();
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];

  // If the application is not yet ready to present the UI, install
  // a LaunchScreenViewController as the root view of the connected
  // SceneState. This ensures that there is no "blank" window.
  if (self.appState.initStage < AppInitStage::kBrowserObjectsForUI) {
    LaunchScreenViewController* launchScreen =
        [[LaunchScreenViewController alloc] init];
    [sceneState setRootViewController:launchScreen makeKeyAndVisible:YES];
  }

  if (self.appState.initStage >= AppInitStage::kNormalUI) {
    [self attachProfileToSceneState:sceneState];
  }
}

// Called when the first scene becomes active.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  DCHECK(self.appState.initStage > AppInitStage::kSafeMode);

  if (self.appState.initStage <= AppInitStage::kNormalUI) {
    return;
  }

  // TODO(crbug.com/40769058): Pass the scene to this method to make sure that
  // the chosen scene is initialized.
  [self startUpAfterFirstWindowCreated];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  // TODO(crbug.com/40769058): Remove this once the bug fixed.
  if (previousInitStage == AppInitStage::kNormalUI &&
      appState.firstSceneHasInitializedUI) {
    [self startUpAfterFirstWindowCreated];
  }

  switch (appState.initStage) {
    case AppInitStage::kStart:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kBrowserBasic:
      [self startUpBrowserBasicInitialization];
      break;
    case AppInitStage::kSafeMode:
      [self addPostSafeModeAgents];
      break;
    case AppInitStage::kVariationsSeed:
      break;
    case AppInitStage::kBrowserObjectsForBackgroundHandlers:
      [self startUpBrowserBackgroundInitialization];
      break;
    case AppInitStage::kEnterprise:
      break;
    case AppInitStage::kLoadProfiles:
      [self startUpBrowserBackgroundProfilesInitialization];
      break;
    case AppInitStage::kBrowserObjectsForUI:
      [self maybeContinueForegroundInitialization];
      break;
    case AppInitStage::kNormalUI:
      break;
    case AppInitStage::kFirstRun:
      break;
    case AppInitStage::kChoiceScreen:
      break;
    case AppInitStage::kFinal:
      break;
  }
}

- (void)addPostSafeModeAgents {
  [self.appState addAgent:[[EnterpriseAppAgent alloc] init]];
  [self.appState addAgent:[[IncognitoUsageAppStateAgent alloc] init]];
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  [self.appState addAgent:[[CredentialProviderMigratorAppAgent alloc] init]];
#endif
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    [profileState removeObserver:self];
    [self recordLaunchMetrics];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelDisconnected) {
    [sceneState removeObserver:self];
  } else if (level > SceneActivationLevelBackground) {
    // Stop observing all scenes since we only needed to know when the app
    // (first scene) is about to go to the foreground.
    for (SceneState* scene in _appState.connectedScenes) {
      [scene removeObserver:self];
    }
    [self maybeContinueForegroundInitialization];
  }
}

#pragma mark - Property implementation.

- (void)setAppState:(AppState*)appState {
  DCHECK(!_appState);
  _appState = appState;
  [appState addObserver:self];

  _scopedForceOrientation = ForcePortraitOrientationOnIphone(_appState);

  // Create app state agents.
  [appState addAgent:[[AppMetricsAppStateAgent alloc] init]];
  [appState addAgent:[[SafeModeAppAgent alloc] init]];
  [appState addAgent:[[VariationsAppStateAgent alloc] init]];

  BackgroundRefreshAppAgent* refreshAgent =
      [[BackgroundRefreshAppAgent alloc] init];
  refreshAgent.startupInformation = self;
  [_appState addAgent:refreshAgent];
  // Register background refresh providers.
  [refreshAgent addAppRefreshProvider:[[TestRefresher alloc]
                                          initWithAppState:self.appState]];

  // TODO(crbug.com/355142171): Remove the DiscoverFeedAppAgent.
  [appState addAgent:[[DiscoverFeedAppAgent alloc] init]];

  // Create the window accessibility agent only when multiple windows are
  // possible.
  if (base::ios::IsMultipleScenesSupported()) {
    [appState addAgent:[[WindowAccessibilityChangeNotifierAppAgent alloc] init]];
  }
}

// TODO(crbug.com/341906612): Get rid of this method/property completely.
- (id<BrowserProviderInterface>)browserProviderInterfaceDoNotUse {
  if (self.appState.foregroundActiveScene) {
    return self.appState.foregroundActiveScene.browserProviderInterface;
  }
  NSArray<SceneState*>* connectedScenes = self.appState.connectedScenes;

  return connectedScenes.count == 0
             ? nil
             : connectedScenes[0].browserProviderInterface;
}

- (BOOL)isFirstLaunchAfterUpgrade {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

#pragma mark - StartupInformation implementation.


- (FirstUserActionRecorder*)firstUserActionRecorder {
  return _firstUserActionRecorder.get();
}

- (void)resetFirstUserActionRecorder {
  _firstUserActionRecorder.reset();
}

- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay {
  [self performSelector:@selector(expireFirstUserActionRecorder)
             withObject:nil
             afterDelay:delay];
}

- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime {
  base::TimeDelta delta = base::Seconds(backgroundTime);
  _firstUserActionRecorder.reset(new FirstUserActionRecorder(delta));
}

- (void)stopChromeMain {
  for (SpotlightManager* spotlightManager : _spotlightManagers) {
    [spotlightManager shutdown];
  }
  [_spotlightManagers removeAllObjects];

  // _localStatePrefChangeRegistrar is observing the local state PrefService,
  // which is owned indirectly by _chromeMain (through the ApplicationContext).
  // Unregister the observer before the ApplicationContext is destroyed.
  _localStatePrefChangeRegistrar.RemoveAll();

  // Inform all the ProfileControllers that they will be destroyed in order
  // to allow them to perform all required cleanup before the application
  // terminates.
  for (const auto& pair : _profileControllers) {
    ProfileController* controller = pair.second;
    [controller shutdown];
  }

  _profileControllers.clear();

  // Cancel any pending deferred startup tasks (the application is shutting
  // down, so there is no point in running them).
  [_appState.deferredRunner cancelAllBlocks];

#if BUILDFLAG(FAST_APP_TERMINATE_ENABLED)
  // _chromeMain.reset() is a blocking call that regularly causes
  // applicationWillTerminate to fail after a 5s delay. Experiment with skipping
  // this shutdown call. See: crbug.com/1328891
  if (base::FeatureList::IsEnabled(kFastApplicationWillTerminate)) {
    // Expected number of time the `closure` defined below needs to
    // be called before it signal the semaphore. This corresponds to the
    // number of services that needs to be waited for.
    uint32_t expectedCount = 0;

    // MetricsService doesn't depend on a profile.
    metrics::MetricsService* metrics =
        GetApplicationContext()->GetMetricsService();
    if (metrics) {
      expectedCount += 1;
    }

    const std::vector<ProfileIOS*> loadedProfiles =
        GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
    for (ProfileIOS* profile : loadedProfiles) {
      expectedCount += 1;
      if (profile->HasOffTheRecordProfile()) {
        expectedCount += 1;
      }
    }

    // `dispatch_semaphore_signal` is called only once when `closure` is called
    // `expectedCount` times.
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    base::RepeatingClosure closure =
        base::BarrierClosure(expectedCount, base::BindOnce(^{
                               dispatch_semaphore_signal(semaphore);
                             }));

    for (ProfileIOS* profile : loadedProfiles) {
      SessionRestorationServiceFactory::GetForProfile(profile)
          ->InvokeClosureWhenBackgroundProcessingDone(closure);

      if (profile->HasOffTheRecordProfile()) {
        ProfileIOS* otrBrowserState = profile->GetOffTheRecordProfile();
        SessionRestorationServiceFactory::GetForProfile(otrBrowserState)
            ->InvokeClosureWhenBackgroundProcessingDone(closure);
      }
    }

    if (metrics) {
      metrics->Stop();
      // MetricsService::Stop() depends on a committed local state, and does
      // so asynchronously. To avoid losing metrics, this minimum wait is
      // required. This will introduce a wait that will likely be the source
      // of a number of watchdog kills, but it should still be fewer than the
      // number of kills `_chromeMain.reset()` is responsible for.
      GetApplicationContext()->GetLocalState()->CommitPendingWrite({}, closure);
    }

    dispatch_time_t dispatchTime =
        dispatch_time(DISPATCH_TIME_NOW, 4 * NSEC_PER_SEC);
    dispatch_semaphore_wait(semaphore, dispatchTime);

    return;
  }
#endif  // BUILDFLAG(FAST_APP_TERMINATE_ENABLED)

  _chromeMain.reset();
}

#pragma mark - Startup tasks

// Continues foreground initialization iff both the init stage and activation
// level are ready.
- (void)maybeContinueForegroundInitialization {
  if (self.appState.foregroundScenes.count > 0 &&
      self.appState.initStage == AppInitStage::kBrowserObjectsForUI) {
    DCHECK(self.appState.userInteracted);
    [self startUpBrowserForegroundInitialization];
    [self.appState queueTransitionToNextInitStage];
  }
}

- (void)sendQueuedFeedback {
  if (ios::provider::IsUserFeedbackSupported()) {
    [_appState.deferredRunner
        enqueueBlockNamed:kSendQueuedFeedback
                    block:^{
                      ios::provider::UploadAllPendingUserFeedback();
                    }];
  }
}

- (void)orientationDidChange:(NSNotification*)notification {
  crash_keys::SetCurrentOrientation(GetInterfaceOrientation(),
                                    [[UIDevice currentDevice] orientation]);
}

- (void)registerForOrientationChangeNotifications {
  // Register device orientation. UI orientation will be registered by
  // each window BVC. These two events may be triggered independently.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationDidChange:)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];
}

- (void)schedulePrefObserverInitialization {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner enqueueBlockNamed:kStartupInitPrefObservers
                                        block:^{
                                          [weakSelf initializePrefObservers];
                                        }];
}

- (void)initializePrefObservers {
  // Track changes to local state prefs.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  _localStatePrefChangeRegistrar.Init(localState);
  _localStatePrefObserverBridge = std::make_unique<PrefObserverBridge>(self);
  _localStatePrefObserverBridge->ObserveChangesForPreference(
      metrics::prefs::kMetricsReportingEnabled,
      &_localStatePrefChangeRegistrar);

  // Calls the onPreferenceChanged function in case there was a change to the
  // observed preferences before the observer bridge was set up. However, if the
  // metrics reporting pref is still unset (has default value), then do not
  // call. This likely means that the user is still on the welcome screen during
  // the first run experience (FRE), and calling onPreferenceChanged here would
  // clear the provisional client ID (in
  // MetricsMediator::updateMetricsPrefsOnPermissionChange). The provisional
  // client ID is crucial for field trial assignment consistency between the
  // first session and follow-up sessions, and is promoted to be the real client
  // ID if the user enables metrics reporting in the FRE. Otherwise, it is
  // discarded, as would happen here if onPreferenceChanged was called while the
  // user was still on the welcome screen and did yet enable/disable metrics
  // reporting.
  if (!localState->FindPreference(metrics::prefs::kMetricsReportingEnabled)
           ->IsDefaultValue()) {
    [self onPreferenceChanged:metrics::prefs::kMetricsReportingEnabled];
  }
}

- (void)scheduleAppDistributionPings {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner enqueueBlockNamed:kSendInstallPingIfNecessary
                                        block:^{
                                          [weakSelf pingDistributionServices];
                                        }];
}

- (void)scheduleStartupAttemptReset {
  [_appState.deferredRunner
      enqueueBlockNamed:kStartupResetAttemptCount
                  block:^{
                    crash_util::ResetFailedStartupAttemptCount();
                  }];
}

- (void)scheduleCrashReportUpload {
  [_appState.deferredRunner enqueueBlockNamed:kUploadCrashReports
                                        block:^{
                                          crash_helper::UploadCrashReports();
                                        }];
}

- (void)scheduleDiscardedSessionsCleanup {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner enqueueBlockNamed:kCleanupDiscardedSessions
                                        block:^{
                                          [weakSelf cleanupDiscardedSessions];
                                        }];
}

- (void)scheduleReadingListDownloadServiceInitialization {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner
      enqueueBlockNamed:kInitializeReadingListDownloadService
                  block:^{
                    [weakSelf initializeReadListDownloadService];
                  }];
}

- (void)scheduleMemoryDebuggingTools {
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    __weak MainController* weakSelf = self;
    [_appState.deferredRunner
        enqueueBlockNamed:kMemoryDebuggingToolsStartup
                    block:^{
                      [weakSelf initializedMemoryDebuggingTools];
                    }];
  }
}

- (void)initializedMemoryDebuggingTools {
  DCHECK(!_memoryDebuggerManager);
  DCHECK(experimental_flags::IsMemoryDebuggingEnabled());
  _memoryDebuggerManager = [[MemoryDebuggerManager alloc]
      initWithView:self.appState.foregroundActiveScene.window
             prefs:GetApplicationContext()->GetLocalState()];
}

// Schedule a call to `scheduleSaveFieldTrialValuesForExternals` for deferred
// execution. Externals can be extensions or 1st party apps.
- (void)scheduleSaveFieldTrialValuesForExternals {
  __weak __typeof(self) weakSelf = self;
  [_appState.deferredRunner
      enqueueBlockNamed:kSaveFieldTrialValues
                  block:^{
                    [weakSelf saveFieldTrialValuesForExtensions];
                    [weakSelf saveFieldTrialValuesForGroupApp];
                  }];
}

// Some experiments value may be useful for first-party applications, so save
// the value in the shared application group.
- (void)saveFieldTrialValuesForGroupApp {
  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSNumber* supportsShowDefaultBrowserPromo =
      @(base::FeatureList::IsEnabled(kDefaultBrowserIntentsShowSettings));

  NSDictionary* capabilities = @{
    app_group::
    kChromeShowDefaultBrowserPromoCapability : supportsShowDefaultBrowserPromo
  };
  [sharedDefaults setObject:capabilities
                     forKey:app_group::kChromeCapabilitiesPreference];
}

// Some extensions need the value of field trials but can't get them because the
// field trial infrastructure isn't in extensions. Save the necessary values to
// NSUserDefaults here.
- (void)saveFieldTrialValuesForExtensions {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  // Add other field trial values here if they are needed by extensions.
  // The general format is
  // {
  //   name: {
  //     value: NSNumber bool,
  //     version: NSNumber int,
  //   }
  // }
  NSDictionary* fieldTrialValues = @{
    kWidgetKitRefreshFiveMinutes : @{
      kFieldTrialValueKey : @([[NSUserDefaults standardUserDefaults]
          boolForKey:kWidgetKitRefreshFiveMinutes]),
      kFieldTrialVersionKey : @1,
    },
  };
  [sharedDefaults setObject:fieldTrialValues
                     forKey:app_group::kChromeExtensionFieldTrialPreference];
}

// Schedules a call to `logIfEnterpriseManagedDevice` for deferred
// execution.
- (void)scheduleEnterpriseManagedDeviceCheck {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner
      enqueueBlockNamed:kEnterpriseManagedDeviceCheck
                  block:^{
                    [weakSelf logIfEnterpriseManagedDevice];
                  }];
}

- (void)logIfEnterpriseManagedDevice {
  NSString* managedKey = @"com.apple.configuration.managed";
  BOOL isManagedDevice = [[NSUserDefaults standardUserDefaults]
                             dictionaryForKey:managedKey] != nil;

  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged2", isManagedDevice);
}

- (void)startFreeMemoryMonitoring {
  // No need for a post-task or a deferred initialisation as the memory
  // monitoring already happens on a background sequence.
  StartFreeMemoryMonitor();
}

- (void)scheduleLowPriorityStartupTasks {
  [_startupTasks initializeOmaha];

  // Deferred tasks.
  [self scheduleMemoryDebuggingTools];
  [self scheduleReadingListDownloadServiceInitialization];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteTempDownloadsDirectory];
  [self scheduleDeleteTempPasswordsDirectory];
  [self scheduleDeleteTempChooseFileDirectory];
  [self scheduleLogSiriShortcuts];
  [self scheduleStartupAttemptReset];
  [self startFreeMemoryMonitoring];
  [self scheduleAppDistributionPings];
  [self scheduleSaveFieldTrialValuesForExternals];
  [self scheduleEnterpriseManagedDeviceCheck];
  [self scheduleMemoryExperimentation];
#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
  [self scheduleDumpDocumentsStatistics];
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
}

- (void)scheduleDeleteTempDownloadsDirectory {
  [_appState.deferredRunner enqueueBlockNamed:kDeleteDownloads
                                        block:^{
                                          DeleteTempDownloadsDirectory();
                                        }];
}

- (void)scheduleDeleteTempChooseFileDirectory {
  [_appState.deferredRunner enqueueBlockNamed:kDeleteChooseFile
                                        block:^{
                                          DeleteTempChooseFileDirectory();
                                        }];
}

- (void)scheduleDeleteTempPasswordsDirectory {
  [_appState.deferredRunner
      enqueueBlockNamed:kDeleteTempPasswords
                  block:^{
                    password_manager::DeletePasswordsDirectory();
                  }];
}

- (void)scheduleMemoryExperimentation {
  [_appState.deferredRunner
      enqueueBlockNamed:kMemoryExperimentation
                  block:^{
                    BeginMemoryExperimentationAfterDelay();
                  }];
}

- (void)scheduleLogSiriShortcuts {
  __weak StartupTasks* startupTasks = _startupTasks;
  [_appState.deferredRunner enqueueBlockNamed:kLogSiriShortcuts
                                        block:^{
                                          [startupTasks logSiriShortcuts];
                                        }];
}

- (void)scheduleSpotlightResync {
  __weak MainController* weakSelf = self;
  [_appState.deferredRunner enqueueBlockNamed:kStartSpotlightBookmarksIndexing
                                        block:^{
                                          [weakSelf resyncIndex];
                                        }];
}

- (void)resyncIndex {
  for (SpotlightManager* manager : _spotlightManagers) {
    [manager resyncIndex];
  }
}

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
- (void)scheduleDumpDocumentsStatistics {
  if ([[NSUserDefaults standardUserDefaults]
          boolForKey:@"EnableDumpSandboxFileStatistics"]) {
    // Reset the pref to prevent dumping statistics on every launch.
    [[NSUserDefaults standardUserDefaults]
        setBool:NO
         forKey:@"EnableDumpSandboxFileStatistics"];

    documents_statistics::DumpSandboxFileStatistics();
  }
}
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

- (void)expireFirstUserActionRecorder {
  // Clear out any scheduled calls to this method. For example, the app may have
  // been backgrounded before the `kFirstUserActionTimeout` expired.
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector(
                                                  expireFirstUserActionRecorder)
                                       object:nil];

  if (_firstUserActionRecorder) {
    _firstUserActionRecorder->Expire();
    _firstUserActionRecorder.reset();
  }
}

- (void)crashIfRequested {
  if (experimental_flags::IsStartupCrashEnabled()) {
    // Flush out the value cached for crash_helper::SetEnabled().
    [[NSUserDefaults standardUserDefaults] synchronize];

    int* x = NULL;
    *x = 0;
  }
}

#pragma mark - Preferences Management

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Turn on or off metrics & crash reporting when either preference changes.
  if (preferenceName == metrics::prefs::kMetricsReportingEnabled) {
    [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:YES];
  }
}

#pragma mark - Helper methods.

- (void)cleanupDiscardedSessions {
  const std::set<std::string> discardedSessionIDs =
      sessions_storage_util::GetDiscardedSessions();
  if (discardedSessionIDs.empty()) {
    return;
  }

  const std::set<std::string> connectedSessionIDs = [self connectedSessionIDs];

  std::set<std::string> identifiers;
  std::set<std::string> postponedRemovals;
  for (const std::string& sessionID : discardedSessionIDs) {
    // TODO(crbug.com/350946190): it looks like it is possible for the OS to
    // inform the application that a scene is discarded even though the scene
    // is still connected. If this happens, postpone the removal until the
    // next execution of the application.
    if (connectedSessionIDs.contains(sessionID)) {
      postponedRemovals.insert(sessionID);
    } else {
      // Need to remove storage for both regular and inactive Browser. Removing
      // data does nothing if there are no data to delete, so there is no need
      // to check whether inactive tabs are enabled here.
      identifiers.insert(session_util::GetSessionIdentifier(sessionID, false));
      identifiers.insert(session_util::GetSessionIdentifier(sessionID, true));
    }
  }

  // If all sessions to discard are still mapped, postpone everything.
  if (identifiers.empty()) {
    return;
  }

  // Will execute the closure passed to `Done()` when all the callbacks have
  // completed.
  base::ConcurrentClosures concurrent;

  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    SessionRestorationServiceFactory::GetForProfile(profile)
        ->DeleteDataForDiscardedSessions(identifiers,
                                         concurrent.CreateClosure());

    if (profile->HasOffTheRecordProfile()) {
      ProfileIOS* otrBrowserState = profile->GetOffTheRecordProfile();
      SessionRestorationServiceFactory::GetForProfile(otrBrowserState)
          ->DeleteDataForDiscardedSessions(identifiers,
                                           concurrent.CreateClosure());
    }
  }

  base::OnceClosure closure =
      base::BindOnce(&sessions_storage_util::ResetDiscardedSessions);
  if (!postponedRemovals.empty()) {
    closure = std::move(closure).Then(
        base::BindOnce(&sessions_storage_util::MarkSessionsForRemoval,
                       std::move(postponedRemovals)));
  }

  std::move(concurrent).Done(std::move(closure));
}

- (void)initializeReadListDownloadService {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    ReadingListDownloadServiceFactory::GetForProfile(profile)->Initialize();
  }
}

- (void)pingDistributionServices {
  const base::Time installDate =
      base::Time::FromTimeT(GetApplicationContext()->GetLocalState()->GetInt64(
          metrics::prefs::kInstallDate));

  auto URLLoaderFactory = GetApplicationContext()->GetSharedURLLoaderFactory();
  const bool isFirstRun = FirstRun::IsChromeFirstRun();
  ios::provider::ScheduleAppDistributionNotifications(URLLoaderFactory,
                                                      isFirstRun);
  ios::provider::InitializeFirebase(installDate, isFirstRun);
}

// Records launch metrics when the application and all initial profiles have
// been fully initialised.
- (void)recordLaunchMetrics {
  // Only record the metrics once, not after dynamically loading a new profile
  // late (e.g. switching profile for a scene or opening a scene with another
  // profile).
  if (_launchMetricsRecorded) {
    return;
  }

  // Check that all profiles have been fully initialized before recording the
  // metrics (since before that, the tabs may have not been loaded yet and thus
  // the metrics can't be properly recorded).
  NSArray<SceneState*>* connectedScenes = self.appState.connectedScenes;
  for (SceneState* sceneState in connectedScenes) {
    if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
      return;
    }
  }

  // As all profiles have been fully initialised, the number of tabs and of
  // connected scenes is now correct and can be reported.
  [MetricsMediator logLaunchMetricsWithStartupInformation:self
                                          connectedScenes:connectedScenes];

  // Avoid reporting the metrics again.
  _launchMetricsRecorded = YES;
}

#pragma mark - BlockingSceneCommands

- (void)activateBlockingScene:(UIScene*)requestingScene {
  id<UIBlockerTarget> uiBlocker = self.appState.currentUIBlocker;
  if (!uiBlocker) {
    return;
  }

  [uiBlocker bringBlockerToFront:requestingScene];
}

#pragma mark - ChangeProfileCommands

- (void)changeProfile:(NSString*)profileName
             forScene:(NSString*)sceneIdentifier
           completion:(ChangeProfileCompletion)completion {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Not supported when kSeparateProfilesForManagedAccounts is disabled or not
    // available.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion, /*success=*/false));
    return;
  }

  SceneState* sceneState = [self sceneForIdentifier:sceneIdentifier];
  if (sceneState == nil) {
    // No scene with that identifier, cannot change the profile.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion, /*success=*/false));
    return;
  }

  ProfileManagerIOS* profileManager =
      GetApplicationContext()->GetProfileManager();

  const std::string wantedProfileName = base::SysNSStringToUTF8(profileName);
  const std::string actualProfileName =
      profileManager->GetProfileAttributesStorage()->GetProfileNameForSceneID(
          base::SysNSStringToUTF8(sceneIdentifier));

  if (actualProfileName == wantedProfileName) {
    auto iter = _profileControllers.find(actualProfileName);
    if (iter != _profileControllers.end()) {
      // The SceneState is already associated with the correct Profile
      // and it is loaded, so wait for the initialisation to complete.
      [ChangeProfileObserver waitForProfile:iter->second.state
                           toReachInitStage:ProfileInitStage::kFinal
                                 completion:completion];
      return;
    }
  }

  // Need to load the Profile and to attach it to the Scene.
  __weak MainController* weakSelf = self;
  profileManager->CreateProfileAsync(wantedProfileName,
                                     base::BindOnce(^(ProfileIOS* profile) {
                                       [weakSelf profileLoaded:profile
                                                 forSceneState:sceneState
                                                    completion:completion];
                                     }));
}

#pragma mark - Private

// Helper method for switching the profile for a scene.
// Called when the profile has been loaded (`profile` is null if loading the
// profile has failed).
- (void)profileLoaded:(ProfileIOS*)profile
        forSceneState:(SceneState*)sceneState
           completion:(ChangeProfileCompletion)completion {
  if (!profile) {
    // Creating the profile failed, cannot change the profile.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion, /*success=*/false));
    return;
  }

  // Initialize the profile if needed.
  if (!base::Contains(_profileControllers, profile->GetProfileName())) {
    [self initializeProfile:profile];
  }

  // Set the mapping between profile and scene.
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->SetProfileNameForSceneID(
          base::SysNSStringToUTF8(sceneState.sceneSessionID),
          profile->GetProfileName());

  // Pretend the scene has been disconnected, then reconnect it.
  const SceneActivationLevel savedLevel = sceneState.activationLevel;
  const WindowActivityOrigin savedOrigin = sceneState.currentOrigin;
  UISceneConnectionOptions* savedConnectionOptions =
      sceneState.connectionOptions;

  // Get the SceneDelegate from the SceneState.
  UIWindowScene* scene = sceneState.scene;
  SceneDelegate* sceneDelegate =
      base::apple::ObjCCast<SceneDelegate>(scene.delegate);
  DCHECK(sceneDelegate);

  // Install a new root view controller before destroying the UI (since it
  // does not support dismissing the root view controller after the Browser
  // has been destroyed).
  // TODO(crbug.com/376667510): SceneDelegate should manage the view controller
  // and this should be unnecessary (in fact, it should be possible to install
  // a temporary view controller to perform an animation).
  LaunchScreenViewController* launchScreen =
      [[LaunchScreenViewController alloc] init];
  [sceneState setRootViewController:launchScreen makeKeyAndVisible:YES];

  [sceneDelegate sceneDidDisconnect:scene];  // destroy the old SceneState
  sceneState = sceneDelegate.sceneState;     // recreate a new SceneState
  sceneState.currentOrigin = savedOrigin;
  sceneState.connectionOptions = savedConnectionOptions;
  sceneState.activationLevel = SceneActivationLevelBackground;
  sceneState.scene = scene;

  [self appState:self.appState sceneConnected:sceneState];
  DCHECK_EQ(sceneState.profileState.profile, profile);

  while (sceneState.activationLevel < savedLevel) {
    sceneState.activationLevel = static_cast<SceneActivationLevel>(
        base::to_underlying(sceneState.activationLevel) + 1);
  }

  [ChangeProfileObserver waitForProfile:sceneState.profileState
                       toReachInitStage:ProfileInitStage::kFinal
                             completion:completion];
}

// Returns the SceneState with the given `sceneIdentifier`.
- (SceneState*)sceneForIdentifier:(NSString*)sceneIdentifier {
  for (SceneState* sceneState in self.appState.connectedScenes) {
    if ([sceneState.sceneSessionID isEqualToString:sceneIdentifier]) {
      return sceneState;
    }
  }

  return nil;
}

// Returns the set of Session identifiers for all connected scenes.
- (std::set<std::string>)connectedSessionIDs {
  std::set<std::string> connectedSessionIDs;
  for (SceneState* sceneState in self.appState.connectedScenes) {
    connectedSessionIDs.insert(
        base::SysNSStringToUTF8(sceneState.sceneSessionID));
  }
  return connectedSessionIDs;
}

- (void)attachProfileToSceneState:(SceneState*)sceneState {
  ProfileAttributesStorageIOS* storage = GetApplicationContext()
                                             ->GetProfileManager()
                                             ->GetProfileAttributesStorage();

  const std::string sceneID =
      base::SysNSStringToUTF8(sceneState.sceneSessionID);
  std::string profileName = storage->GetProfileNameForSceneID(sceneID);

  auto iterator = _profileControllers.find(profileName);
  if (iterator == _profileControllers.end()) {
    if (profileName.empty()) {
      // TODO(crbug.com/41492447): provide an API to mark a profile as the
      // profile to use by default when a new SceneState is open.
      profileName = GetApplicationContext()->GetLocalState()->GetString(
          prefs::kLastUsedProfile);
      if (profileName.empty()) {
        profileName = kIOSChromeInitialProfile;
      }

      iterator = _profileControllers.find(profileName);
      storage->SetProfileNameForSceneID(sceneID, profileName);
    }

    DCHECK(!profileName.empty());
    if (iterator == _profileControllers.end()) {
      __weak __typeof(self) weakSelf = self;
      GetApplicationContext()->GetProfileManager()->CreateProfileAsync(
          profileName, base::BindOnce(^(ProfileIOS* profile) {
            [weakSelf profileLoaded:profile forSceneState:sceneState];
          }));
      return;
    }
  }

  DCHECK(iterator != _profileControllers.end());
  ProfileState* profileState = iterator->second.state;

  // TODO(crbug.com/343166723): remove the global mainProfile when it is only
  // accessed per scene.
  if (!self.appState.mainProfile) {
    self.appState.mainProfile = profileState;
  }

  // TODO(crbug.com/353683675) Improve this logic once ProfileInitStage and
  // AppInitStage are fully decoupled.
  AppInitStage initStage = self.appState.initStage;
  if (initStage >= AppInitStage::kLoadProfiles) {
    ProfileInitStage currStage = profileState.initStage;
    ProfileInitStage nextStage = ProfileInitStageFromAppInitStage(initStage);
    while (currStage != nextStage) {
      // The ProfileInitStage enum has more values than AppInitStage, so move
      // over all stage that have no representation in AppInitStage to avoid
      // failing CHECK in -[ProfileState setInitStage:].
      currStage =
          static_cast<ProfileInitStage>(base::to_underlying(currStage) + 1);
      profileState.initStage = currStage;
    }
  }

  [sceneState.controller setProfileState:profileState];
  storage->SetProfileNameForSceneID(sceneID, iterator->first);
}

// TODO(crbug.com/353683675) Improve this logic once ProfileInitStage and
// AppInitStage are fully decoupled.
- (void)profileLoaded:(ProfileIOS*)profile
        forSceneState:(SceneState*)sceneState {
  [self initializeProfile:profile];
  [self attachProfileToSceneState:sceneState];
}

@end
