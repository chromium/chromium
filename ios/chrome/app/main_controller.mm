// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#import <memory>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/installer_policies/autofill_states_component_installer.h"
#import "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#import "components/component_updater/installer_policies/optimization_hints_component_installer.h"
#import "components/component_updater/installer_policies/safety_tips_component_installer.h"
#import "components/component_updater/url_param_filter_remover.h"
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
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_storage_metrics.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/enterprise_app_agent.h"
#import "ios/chrome/app/fast_app_terminate_buildflags.h"
#import "ios/chrome/app/features.h"
#import "ios/chrome/app/feed_app_agent.h"
#import "ios/chrome/app/first_run_app_state_agent.h"
#import "ios/chrome/app/memory_monitor.h"
#import "ios/chrome/app/post_restore_app_agent.h"
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
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service_factory.h"
#import "ios/chrome/browser/memory/model/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/incognito_usage_app_state_agent.h"
#import "ios/chrome/browser/metrics/window_configuration_recorder.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/screenshot/model/screenshot_metrics_recorder.h"
#import "ios/chrome/browser/search_engines/model/extension_search_engine_data_updater.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/certificate_policy_app_agent.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_field_trial_version.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/empty_nsurlcache.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/public/provider/chrome/browser/overrides/overrides_api.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/features.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "net/base/mac/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/app/credential_provider_migrator_app_agent.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_service_factory.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_support.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#endif

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#import "ios/chrome/app/dump_documents_statistics.h"
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {

#if BUILDFLAG(FAST_APP_TERMINATE_ENABLED)
// Skip chromeMain.reset() on shutdown, see crbug.com/1328891 for details.
BASE_FEATURE(kFastApplicationWillTerminate,
             "FastApplicationWillTerminate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(FAST_APP_TERMINATE_ENABLED)

// Constants for deferring resetting the startup attempt count (to give the app
// a little while to make sure it says alive).
NSString* const kStartupAttemptReset = @"StartupAttemptReset";

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

// Constant for deferring the cleanup of discarded sessions on disk.
NSString* const kCleanupDiscardedSessions = @"CleanupDiscardedSessions";

// Constants for deferring mailto handling initialization.
NSString* const kMailtoHandlingInitialization = @"MailtoHandlingInitialization";

// Constants for deferring saving field trial values
NSString* const kSaveFieldTrialValues = @"SaveFieldTrialValues";

// Constants for refreshing the WidgetKit after five minutes
NSString* const kWidgetKitRefreshFiveMinutes = @"WidgetKitRefreshFiveMinutes";

// Constants for deferred check if it is necessary to send pings to
// Chrome distribution related services.
NSString* const kSendInstallPingIfNecessary = @"SendInstallPingIfNecessary";

// Constants for deferred deletion of leftover user downloaded files.
NSString* const kDeleteDownloads = @"DeleteDownloads";

// Constants for deferred deletion of leftover temporary passwords files.
NSString* const kDeleteTempPasswords = @"DeleteTempPasswords";

// Constants for deferred UMA logging of existing Siri User shortcuts.
NSString* const kLogSiriShortcuts = @"LogSiriShortcuts";

// Constants for deferred sending of queued feedback.
NSString* const kSendQueuedFeedback = @"SendQueuedFeedback";

// Constants for deferring the upload of crash reports.
NSString* const kUploadCrashReports = @"UploadCrashReports";

// Constants for deferring the cleanup of snapshots on disk.
NSString* const kCleanupSnapshots = @"CleanupSnapshots";

// Constants for deferring startup Spotlight bookmark indexing.
NSString* const kStartSpotlightBookmarksIndexing =
    @"StartSpotlightBookmarksIndexing";

// Constants for deferring the enterprise managed device check.
NSString* const kEnterpriseManagedDeviceCheck = @"EnterpriseManagedDeviceCheck";

// Constants for deferred deletion of leftover session state files.
NSString* const kPurgeWebSessionStates = @"PurgeWebSessionStates";

// Constants for deferred favicons clean up.
NSString* const kFaviconsCleanup = @"FaviconsCleanup";

// The minimum amount of time (2 weeks in seconds) between calculating and
// logging metrics about the amount of device storage space used by Chrome.
const NSTimeInterval kMinimumTimeBetweenDocumentsSizeLogging =
    60.0 * 60.0 * 24.0 * 14.0;

// Adapted from chrome/browser/ui/browser_init.cc.
void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      GetApplicationContext()->GetComponentUpdateService();
  DCHECK(cus);
  base::FilePath path;
  const bool success = base::PathService::Get(ios::DIR_USER_DATA, &path);
  DCHECK(success);
  component_updater::DeleteUrlParamFilter(path);

  RegisterOnDeviceHeadSuggestComponent(
      cus, GetApplicationContext()->GetApplicationLocale());
  RegisterSafetyTipsComponent(cus);
  RegisterAutofillStatesComponent(cus,
                                  GetApplicationContext()->GetLocalState());
  RegisterOptimizationHintsComponent(cus);
}

// The delay, in seconds, for cleaning external files.
const int kExternalFilesCleanupDelaySeconds = 60;

// Delegate for the AuthenticationService.
class MainControllerAuthenticationServiceDelegate
    : public AuthenticationServiceDelegate {
 public:
  MainControllerAuthenticationServiceDelegate(
      ChromeBrowserState* browser_state,
      id<BrowsingDataCommands> dispatcher);

  MainControllerAuthenticationServiceDelegate(
      const MainControllerAuthenticationServiceDelegate&) = delete;
  MainControllerAuthenticationServiceDelegate& operator=(
      const MainControllerAuthenticationServiceDelegate&) = delete;

  ~MainControllerAuthenticationServiceDelegate() override;

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(ProceduralBlock completion) override;

 private:
  ChromeBrowserState* browser_state_ = nullptr;
  __weak id<BrowsingDataCommands> dispatcher_ = nil;
};

MainControllerAuthenticationServiceDelegate::
    MainControllerAuthenticationServiceDelegate(
        ChromeBrowserState* browser_state,
        id<BrowsingDataCommands> dispatcher)
    : browser_state_(browser_state), dispatcher_(dispatcher) {}

MainControllerAuthenticationServiceDelegate::
    ~MainControllerAuthenticationServiceDelegate() = default;

void MainControllerAuthenticationServiceDelegate::ClearBrowsingData(
    ProceduralBlock completion) {
  [dispatcher_
      removeBrowsingDataForBrowserState:browser_state_
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:completion];
}

}  // namespace

@interface MainController () <PrefObserverDelegate,
                              BlockingSceneCommands,
                              SceneStateObserver> {
  IBOutlet UIWindow* _window;

  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;

  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _localStatePrefObserverBridge;

  // Registrar for pref changes notifications to the local state.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // Updates data about the current default search engine to be accessed in
  // extensions.
  std::unique_ptr<ExtensionSearchEngineDataUpdater>
      _extensionSearchEngineDataUpdater;

  // The class in charge of showing/hiding the memory debugger when the
  // appropriate pref changes.
  MemoryDebuggerManager* _memoryDebuggerManager;

  // Responsible for indexing chrome links (such as bookmarks, most likely...)
  // in system Spotlight index.
  SpotlightManager* _spotlightManager;

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  WindowConfigurationRecorder* _windowConfigurationRecorder;

  // Handler for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;
}

// Handles collecting metrics on user triggered screenshots
@property(nonatomic, strong)
    ScreenshotMetricsRecorder* screenshotMetricsRecorder;
// Cleanup snapshots on disk.
- (void)cleanupSnapshots;
// Cleanup discarded sessions on disk.
- (void)cleanupDiscardedSessions;
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
// Asynchronously schedules the cleanup of snapshots on disk.
- (void)scheduleSnapshotsCleanup;
// Schedules various cleanup tasks that are performed after launch.
- (void)scheduleStartupCleanupTasks;
// Schedules various tasks to be performed after the application becomes active.
- (void)scheduleLowPriorityStartupTasks;
// Schedules tasks that require a fully-functional BVC to be performed.
- (void)scheduleTasksRequiringBVCWithBrowserState;
// Schedules the deletion of user downloaded files that might be leftover
// from the last time Chrome was run.
- (void)scheduleDeleteTempDownloadsDirectory;
// Schedule the deletion of the temporary passwords files that might
// be left over from incomplete export operations.
- (void)scheduleDeleteTempPasswordsDirectory;
// Crashes the application if requested.
- (void)crashIfRequested;
// Performs synchronous browser state initialization steps.
- (void)initializeBrowserState:(ChromeBrowserState*)browserState;
// Initializes the application to the minimum initialization needed in all
// cases.
- (void)startUpBrowserBasicInitialization;
//  Initializes the browser objects for the background handlers.
- (void)startUpBrowserBackgroundInitialization;
// Initializes the browser objects for the browser UI (e.g., the browser
// state).
- (void)startUpBrowserForegroundInitialization;
@end

@implementation MainController

// Defined by public protocols.
// - StartupInformation
@synthesize isColdStart = _isColdStart;
@synthesize appLaunchTime = _appLaunchTime;
@synthesize isFirstRun = _isFirstRun;
@synthesize didFinishLaunchingTime = _didFinishLaunchingTime;
@synthesize firstSceneConnectionTime = _firstSceneConnectionTime;

#pragma mark - Application lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _isFirstRun = ShouldPresentFirstRunExperience();
    _startupTasks = [[StartupTasks alloc] init];
  }
  return self;
}

- (void)dealloc {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)startUpBrowserBasicInitialization {
  _appLaunchTime = IOSChromeMain::StartTime();
  _isColdStart = YES;
  if (@available(iOS 15, *)) {
    UMA_HISTOGRAM_BOOLEAN("IOS.Process.ActivePrewarm",
                          base::ios::IsApplicationPreWarmed());
  }

  [SetupDebugging setUpDebuggingOptions];

  // Register all providers before calling any Chromium code.
  [ProviderRegistration registerProviders];

  // Start dispatching for blocking UI commands.
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BlockingSceneCommands)];
  [self.appState.appCommandDispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(BrowsingDataCommands)];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(self.appState.initStage > InitStageSafeMode);

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

  ChromeBrowserState* chromeBrowserState = GetApplicationContext()
                                               ->GetChromeBrowserStateManager()
                                               ->GetLastUsedBrowserState();

  // Initialize and set the main browser state.
  [self initializeBrowserState:chromeBrowserState];
  self.appState.mainBrowserState = chromeBrowserState;

  // Give tests a chance to prepare for testing.
  tests_hook::SetUpTestsIfPresent();

  // Force an obvious initialization of the AuthenticationService. This must
  // be done before creation of the UI to ensure the service is initialised
  // before use (it is a security issue, so accessing the service CHECKs if
  // this is not the case). It is important to do this during background
  // initialization when the app is cold started directly into the background
  // because it is used by the DiscoverFeedService, which is started in the
  // background to perform background refresh. There is no downside to doing
  // this during background initialization when the app is launched into the
  // foreground.
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      self.appState.mainBrowserState,
      std::make_unique<MainControllerAuthenticationServiceDelegate>(
          self.appState.mainBrowserState, self));

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
  // TODO(crbug/1232027): Stop watching for a crash if this is a background
  // fetch.
  if (_appState.userInteracted)
    GetApplicationContext()->GetMetricsService()->OnAppEnterForeground();

  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());

  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];

  ChromeBrowserState* browserState = self.appState.mainBrowserState;
  DCHECK(browserState);
  [self.appState
      addAgent:
          [[PostRestoreAppAgent alloc]
              initWithPromosManager:PromosManagerFactory::GetForBrowserState(
                                        browserState)
              authenticationService:AuthenticationServiceFactory::
                                        GetForBrowserState(browserState)
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
                         localState:GetApplicationContext()->GetLocalState()]];
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

#if !defined(NDEBUG)
  // Legacy code used GetLastUsedBrowserState() in this method. We changed it to
  // use self.appState.mainBrowserState instead. The DCHECK ensures that
  // invariant holds true.
  ChromeBrowserState* chromeBrowserState = GetApplicationContext()
                                               ->GetChromeBrowserStateManager()
                                               ->GetLastUsedBrowserState();
  DCHECK_EQ(chromeBrowserState, self.appState.mainBrowserState);
#endif  // !defined(NDEBUG)

  if (!base::ios::IsMultipleScenesSupported()) {
    NSSet<NSString*>* previousSessions =
        [PreviousSessionInfo sharedInstance].connectedSceneSessionsIDs;
    DCHECK(previousSessions.count <= 1);
    self.appState.previousSingleWindowSessionID = [previousSessions anyObject];
  }
  [[PreviousSessionInfo sharedInstance] resetConnectedSceneSessionIDs];

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  // Send "Chrome Opened" event to the feature_engagement::Tracker on cold
  // start.
  tracker->NotifyEvent(feature_engagement::events::kChromeOpened);

  // Send "default_browser_video_promo_conditions_met" event to the
  // feature_engagement::Tracker on cold start.
  if (HasAppLaunchedOnColdStartAndRecordsLaunch()) {
    tracker->NotifyEvent(
        feature_engagement::events::kDefaultBrowserVideoPromoConditionsMet);
  }

  _spotlightManager = [SpotlightManager
      spotlightManagerWithBrowserState:self.appState.mainBrowserState];

  ShareExtensionService* service =
      ShareExtensionServiceFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  service->Initialize();

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  if (IsCredentialProviderExtensionSupported()) {
    CredentialProviderServiceFactory::GetForBrowserState(
        self.appState.mainBrowserState);
  }
#endif

  _windowConfigurationRecorder = [[WindowConfigurationRecorder alloc] init];
}

// This initialization must only happen once there's at least one Chrome window
// open.
- (void)startUpAfterFirstWindowCreated {
  // "Low priority" tasks
  [_startupTasks registerForApplicationWillResignActiveNotification];
  [self registerForOrientationChangeNotifications];

  [self scheduleTasksRequiringBVCWithBrowserState];

  CustomizeUIAppearance();

  [self scheduleStartupCleanupTasks];
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self
                             connectedScenes:self.appState.connectedScenes];

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
  // TODO(crbug/1232027): Determine whether Chrome needs to resume watching for
  // crashes.

  self.appState.postCrashAction = [self postCrashAction];
  [self startUpBeforeFirstWindowCreated];
  base::UmaHistogramEnumeration("Stability.IOS.PostCrashAction",
                                self.appState.postCrashAction);
}

- (void)initializeBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(!browserState->IsOffTheRecord());
  search_engines::UpdateSearchEnginesIfNeeded(
      browserState->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

// Called when the first scene becomes active.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  DCHECK(self.appState.initStage > InitStageSafeMode);

  if (self.appState.initStage <= InitStageNormalUI) {
    return;
  }

  // TODO(crbug.com/1213955): Pass the scene to this method to make sure that
  // the chosen scene is initialized.
  [self startUpAfterFirstWindowCreated];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  // TODO(crbug.com/1213955): Remove this once the bug fixed.
  if (previousInitStage == InitStageNormalUI &&
      appState.firstSceneHasInitializedUI) {
    [self startUpAfterFirstWindowCreated];
  }

  switch (appState.initStage) {
    case InitStageStart:
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageBrowserBasic:
      [self startUpBrowserBasicInitialization];
      break;
    case InitStageSafeMode:
      [self addPostSafeModeAgents];
      break;
    case InitStageVariationsSeed:
      break;
    case InitStageBrowserObjectsForBackgroundHandlers:
      [self startUpBrowserBackgroundInitialization];
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageEnterprise:
      break;
    case InitStageBrowserObjectsForUI:
      [self maybeContinueForegroundInitialization];
      break;
    case InitStageNormalUI:
      // Scene controllers use this stage to create the normal UI if needed.
      // There is no specific agent (other than SceneController) handling
      // this stage.
      [appState queueTransitionToNextInitStage];
      break;
    case InitStageFirstRun:
      break;
    case InitStageFinal:
      break;
  }
}

- (void)addPostSafeModeAgents {
  [self.appState addAgent:[[EnterpriseAppAgent alloc] init]];
  [self.appState addAgent:[[IncognitoUsageAppStateAgent alloc] init]];
  [self.appState addAgent:[[FirstRunAppAgent alloc] init]];
  [self.appState addAgent:[[CertificatePolicyAppAgent alloc] init]];
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  [self.appState addAgent:[[CredentialProviderAppAgent alloc] init]];
#endif
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

  // Create app state agents.
  [appState addAgent:[[AppMetricsAppStateAgent alloc] init]];
  [appState addAgent:[[SafeModeAppAgent alloc] init]];
  [appState addAgent:[[FeedAppAgent alloc] init]];
  [appState addAgent:[[VariationsAppStateAgent alloc] init]];

  // Create the window accessibility agent only when multiple windows are
  // possible.
  if (base::ios::IsMultipleScenesSupported()) {
    [appState addAgent:[[WindowAccessibilityChangeNotifierAppAgent alloc] init]];
  }
}

- (id<BrowserProviderInterface>)browserProviderInterface {
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
  [_spotlightManager shutdown];
  _spotlightManager = nil;

  _extensionSearchEngineDataUpdater = nullptr;

  // _localStatePrefChangeRegistrar is observing the PrefService, which is owned
  // indirectly by _chromeMain (through the ChromeBrowserState).
  // Unregister the observer before the service is destroyed.
  _localStatePrefChangeRegistrar.RemoveAll();

  // Under the UIScene API, the scene delegate does not receive
  // sceneDidDisconnect: notifications on app termination. We mark remaining
  // connected scene states as disconnected in order to allow services to
  // properly unregister their observers and tear down remaining UI.
  for (SceneState* sceneState in self.appState.connectedScenes) {
    sceneState.activationLevel = SceneActivationLevelDisconnected;
  }

#if BUILDFLAG(FAST_APP_TERMINATE_ENABLED)
  // _chromeMain.reset() is a blocking call that regularly causes
  // applicationWillTerminate to fail after a 5s delay. Experiment with skipping
  // this shutdown call. See: crbug.com/1328891
  if (base::FeatureList::IsEnabled(kFastApplicationWillTerminate)) {
    metrics::MetricsService* metrics =
        GetApplicationContext()->GetMetricsService();
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    static std::atomic<uint32_t> counter{metrics ? 2u : 1u};
    ProceduralBlock completionBlock = ^{
      if (!--counter) {
        dispatch_semaphore_signal(semaphore);
      }
    };
    if (!web::features::UseSessionSerializationOptimizations()) {
      [[SessionServiceIOS sharedService]
          shutdownWithCompletion:completionBlock];
    } else {
      completionBlock();
    }

    if (metrics) {
      metrics->Stop();
      // MetricsService::Stop() depends on a committed local state, and does so
      // asynchronously. To avoid losing metrics, this minimum wait is required.
      // This will introduce a wait that will likely be the source of a number
      // of watchdog kills, but it should still be fewer than the number of
      // kills `_chromeMain.reset()` is responsible for.
      GetApplicationContext()->GetLocalState()->CommitPendingWrite(
          {}, base::BindOnce(completionBlock));
      dispatch_time_t dispatchTime =
          dispatch_time(DISPATCH_TIME_NOW, 4 * NSEC_PER_SEC);
      dispatch_semaphore_wait(semaphore, dispatchTime);
    }

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
      self.appState.initStage == InitStageBrowserObjectsForUI) {
    DCHECK(self.appState.userInteracted);
    [self startUpBrowserForegroundInitialization];
    [self.appState queueTransitionToNextInitStage];
  }
}

- (void)sendQueuedFeedback {
  if (ios::provider::IsUserFeedbackSupported()) {
    [[DeferredInitializationRunner sharedInstance]
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
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPrefObserverInit
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

  // Track changes to default search engine.
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.appState.mainBrowserState);
  _extensionSearchEngineDataUpdater =
      std::make_unique<ExtensionSearchEngineDataUpdater>(service);
}

- (void)scheduleAppDistributionPings {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kSendInstallPingIfNecessary
                  block:^{
                    auto URLLoaderFactory = self.appState.mainBrowserState
                                                ->GetSharedURLLoaderFactory();

                    const bool is_first_run = FirstRun::IsChromeFirstRun();
                    ios::provider::ScheduleAppDistributionNotifications(
                        URLLoaderFactory, is_first_run);

                    const base::Time install_date = base::Time::FromTimeT(
                        GetApplicationContext()->GetLocalState()->GetInt64(
                            metrics::prefs::kInstallDate));

                    ios::provider::InitializeFirebase(install_date,
                                                      is_first_run);
                  }];
}

- (void)scheduleStartupAttemptReset {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartupAttemptReset
                  block:^{
                    crash_util::ResetFailedStartupAttemptCount();
                  }];
}

- (void)scheduleCrashReportUpload {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kUploadCrashReports
                  block:^{
                    crash_helper::UploadCrashReports();
                  }];
}

- (void)scheduleDiscardedSessionsCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupDiscardedSessions
                  block:^{
                    [self cleanupDiscardedSessions];
                  }];
}

- (void)scheduleSnapshotsCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kCleanupSnapshots
                  block:^{
                    [self cleanupSnapshots];
                  }];
}

- (void)scheduleSessionStateCacheCleanup {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kPurgeWebSessionStates
                  block:^{
                    if (web::UseNativeSessionRestorationCache()) {
                      WebSessionStateCache* cache =
                          WebSessionStateCacheFactory::GetForBrowserState(
                              self.appState.mainBrowserState);
                      [cache purgeUnassociatedData];
                    }
                  }];
}

- (void)scheduleStartupCleanupTasks {
  // Schedule the prefs observer init first to ensure kMetricsReportingEnabled
  // is synced before starting uploads.
  [self schedulePrefObserverInitialization];
  [self scheduleCrashReportUpload];

  // ClearSessionCookies() is not synchronous.
  if (cookie_util::ShouldClearSessionCookies()) {
    cookie_util::ClearSessionCookies(
        self.appState.mainBrowserState->GetOriginalChromeBrowserState());
    Browser* otrBrowser =
        self.browserProviderInterface.incognitoBrowserProvider.browser;
    if (otrBrowser && !(otrBrowser->GetWebStateList()->empty())) {
      cookie_util::ClearSessionCookies(
          self.appState.mainBrowserState->GetOffTheRecordChromeBrowserState());
    }
  }

  // Remove all discarded sessions from disk.
  [self scheduleDiscardedSessionsCleanup];

  // If the user chooses to restore their session, some cached snapshots and
  // session states may be needed. Otherwise, cleanup the snapshots and session
  // states
  [self scheduleSnapshotsCleanup];
  [self scheduleSessionStateCacheCleanup];
}

- (void)scheduleMemoryDebuggingTools {
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    __weak MainController* weakSelf = self;
    [[DeferredInitializationRunner sharedInstance]
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

- (void)initializeMailtoHandling {
  __weak __typeof(self) weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kMailtoHandlingInitialization
                  block:^{
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    if (!strongSelf || !strongSelf.appState.mainBrowserState) {
                      return;
                    }
                    // Force the creation of the MailtoHandlerService.
                    MailtoHandlerServiceFactory::GetForBrowserState(
                        strongSelf.appState.mainBrowserState);
                  }];
}

// Schedule a call to `scheduleSaveFieldTrialValuesForExternals` for deferred
// execution. Externals can be extensions or 1st party apps.
- (void)scheduleSaveFieldTrialValuesForExternals {
  __weak __typeof(self) weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
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
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kEnterpriseManagedDeviceCheck
                  block:^{
                    [self logIfEnterpriseManagedDevice];
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
  [StartupTasks
      scheduleDeferredBrowserStateInitialization:self.appState
                                                     .mainBrowserState];
  [self sendQueuedFeedback];
  [self scheduleSpotlightResync];
  [self scheduleDeleteTempDownloadsDirectory];
  [self scheduleDeleteTempPasswordsDirectory];
  [self scheduleLogSiriShortcuts];
  [self scheduleStartupAttemptReset];
  [self startFreeMemoryMonitoring];
  [self scheduleAppDistributionPings];
  [self initializeMailtoHandling];
  [self scheduleSaveFieldTrialValuesForExternals];
  [self scheduleEnterpriseManagedDeviceCheck];
  [self scheduleFaviconsCleanup];
  [self scheduleLogDocumentsSize];
#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
  [self scheduleDumpDocumentsStatistics];
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
}

- (void)scheduleTasksRequiringBVCWithBrowserState {
  if (GetApplicationContext()->WasLastShutdownClean()) {
    // Delay the cleanup of the unreferenced files to not impact startup
    // performance.
    ExternalFileRemoverFactory::GetForBrowserState(
        self.appState.mainBrowserState)
        ->RemoveAfterDelay(base::Seconds(kExternalFilesCleanupDelaySeconds),
                           base::OnceClosure());
  }
}

- (void)scheduleDeleteTempDownloadsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteDownloads
                  block:^{
                    DeleteTempDownloadsDirectory();
                  }];
}

- (void)scheduleDeleteTempPasswordsDirectory {
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kDeleteTempPasswords
                  block:^{
                    password_manager::DeletePasswordsDirectory();
                  }];
}

- (void)scheduleLogSiriShortcuts {
  __weak StartupTasks* startupTasks = _startupTasks;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kLogSiriShortcuts
                  block:^{
                    [startupTasks logSiriShortcuts];
                  }];
}

- (void)scheduleSpotlightResync {
  __weak SpotlightManager* spotlightManager = _spotlightManager;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartSpotlightBookmarksIndexing
                  block:^{
                    [spotlightManager resyncIndex];
                  }];
}

- (void)scheduleFaviconsCleanup {
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  __weak MainController* weakSelf = self;
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kFaviconsCleanup
                  block:^{
                    [weakSelf performFaviconsCleanup];
                  }];
#endif
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

- (void)scheduleLogDocumentsSize {
  if (!base::FeatureList::IsEnabled(kLogApplicationStorageSizeMetrics)) {
    return;
  }

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastLogged = base::apple::ObjCCast<NSDate>(
      [defaults objectForKey:kLastApplicationStorageMetricsLogTime]);
  if (lastLogged && [[NSDate date] timeIntervalSinceDate:lastLogged] <
                        kMinimumTimeBetweenDocumentsSizeLogging) {
    return;
  }

  ChromeBrowserState* browserState = self.appState.mainBrowserState;
  base::FilePath profilePath = browserState->GetStatePath();
  base::FilePath offTheRecordStatePath =
      browserState->GetOffTheRecordStatePath();
  LogApplicationStorageMetrics(profilePath, offTheRecordStatePath);
}

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

#pragma mark - Helper methods backed by interfaces.

- (ChromeBrowserState*)currentBrowserState {
  if (!self.browserProviderInterface.currentBrowserProvider.browser) {
    return nullptr;
  }
  return self.browserProviderInterface.currentBrowserProvider.browser
      ->GetBrowserState();
}

- (void)cleanupSnapshots {
  // TODO(crbug.com/1116496): Browsers for disconnected scenes are not in the
  // BrowserList, so this may not reach all folders.
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(self.appState.mainBrowserState);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    SnapshotBrowserAgent::FromBrowser(browser)->PerformStorageMaintenance();
  }
  for (Browser* browser : browser_list->AllIncognitoBrowsers()) {
    SnapshotBrowserAgent::FromBrowser(browser)->PerformStorageMaintenance();
  }
}

- (void)cleanupDiscardedSessions {
  NSArray<NSString*>* sessionIDs =
      sessions_storage_util::GetDiscardedSessions();
  if (!sessionIDs)
    return;
  BrowsingDataRemoverFactory::GetForBrowserState(
      self.appState.mainBrowserState->GetOriginalChromeBrowserState())
      ->RemoveSessionsData(sessionIDs);
  BrowsingDataRemoverFactory::GetForBrowserState(
      self.appState.mainBrowserState->GetOffTheRecordChromeBrowserState())
      ->RemoveSessionsData(sessionIDs);
  sessions_storage_util::ResetDiscardedSessions();
}

#pragma mark - BrowsingDataCommands

- (void)removeBrowsingDataForBrowserState:(ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  BOOL willShowActivityIndicator =
      !browserState->IsOffTheRecord() &&
      IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_SITE_DATA);
  BOOL didShowActivityIndicator = NO;

  for (SceneState* sceneState in self.appState.connectedScenes) {
    // Assumes all scenes share `browserState`.
    id<BrowserProviderInterface> browserProviderInterface =
        sceneState.browserProviderInterface;
    if (willShowActivityIndicator) {
      // Show activity overlay so users know that clear browsing data is in
      // progress.
      if (browserProviderInterface.mainBrowserProvider.browser) {
        didShowActivityIndicator = YES;
        id<BrowserCoordinatorCommands> handler =
            HandlerForProtocol(browserProviderInterface.mainBrowserProvider
                                   .browser->GetCommandDispatcher(),
                               BrowserCoordinatorCommands);
        [handler showActivityOverlay];
      }
    }
  }

  auto removalCompletion = ^{
    // Activates browsing and enables web views.
    // Must be called only on the main thread.
    DCHECK([NSThread isMainThread]);
    for (SceneState* sceneState in self.appState.connectedScenes) {
      // Assumes all scenes share `browserState`.
      id<BrowserProviderInterface> browserProviderInterface =
          sceneState.browserProviderInterface;

      if (willShowActivityIndicator) {
        // User interaction still needs to be disabled as a way to
        // force reload all the web states and to reset NTPs.
        browserProviderInterface.mainBrowserProvider.userInteractionEnabled =
            NO;
        browserProviderInterface.incognitoBrowserProvider
            .userInteractionEnabled = NO;

        if (didShowActivityIndicator &&
            browserProviderInterface.mainBrowserProvider.browser) {
          id<BrowserCoordinatorCommands> handler =
              HandlerForProtocol(browserProviderInterface.mainBrowserProvider
                                     .browser->GetCommandDispatcher(),
                                 BrowserCoordinatorCommands);
          [handler hideActivityOverlay];
        }
      }
      browserProviderInterface.mainBrowserProvider.userInteractionEnabled = YES;
      browserProviderInterface.incognitoBrowserProvider.userInteractionEnabled =
          YES;
      [browserProviderInterface.currentBrowserProvider setPrimary:YES];
    }
    // `completionBlock` is run once, not once per scene.
    if (completionBlock)
      completionBlock();
  };

  BrowsingDataRemoverFactory::GetForBrowserState(browserState)
      ->Remove(timePeriod, removeMask, base::BindOnce(removalCompletion));
}

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
- (void)performFaviconsCleanup {
  ChromeBrowserState* browserState = self.currentBrowserState;
  if (!browserState)
    return;

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  // Only use the fallback to the Google server when fetching favicons for
  // normal encryption users saving to the account, because they are the only
  // users who consented to share data to Google.
  BOOL fallbackToGoogleServer =
      password_manager_util::IsSavingPasswordsToAccountWithNormalEncryption(
          syncService);
  if (fallbackToGoogleServer) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateFaviconsStorageForBrowserState,
                       browserState->AsWeakPtr(), fallbackToGoogleServer));
  }
}
#endif

#pragma mark - BlockingSceneCommands

- (void)activateBlockingScene:(UIScene*)requestingScene {
  id<UIBlockerTarget> uiBlocker = self.appState.currentUIBlocker;
  if (!uiBlocker) {
    return;
  }

  [uiBlocker bringBlockerToFront:requestingScene];
}

@end
