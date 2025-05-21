// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#import <memory>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/barrier_closure.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/functional/concurrent_closures.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/timer/timer.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/installer_policies/autofill_states_component_installer.h"
#import "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#import "components/component_updater/installer_policies/optimization_hints_component_installer.h"
#import "components/component_updater/installer_policies/plus_address_blocklist_component_installer.h"
#import "components/component_updater/installer_policies/safety_tips_component_installer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/passwords_directory_util_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/sync/service/sync_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/app_metrics_app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"
#import "ios/chrome/app/background_refresh/test_refresher.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/change_profile_animator.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/chrome_overlay_window.h"
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
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/main/ui_bundled/browser_view_wrangler.h"
#import "ios/chrome/browser/memory/model/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/model/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/model/incognito_usage_app_state_agent.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/metrics/model/window_configuration_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omaha/model/omaha_service.h"
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/screenshot/model/screenshot_metrics_recorder.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/ui_bundled/chrome_web_ui_ios_controller_factory.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
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
#import "rlz/buildflags/buildflags.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/app/credential_provider_migrator_app_agent.h"
#endif

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#import "ios/chrome/app/dump_documents_statistics.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#import "components/rlz/rlz_tracker.h"                        // nogncheck
#import "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper.h"
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)

namespace {

#if BUILDFLAG(FAST_APP_TERMINATE_ENABLED)
// Skip chromeMain.reset() on shutdown, see crbug.com/1328891 for details.
BASE_FEATURE(kFastApplicationWillTerminate,
             "FastApplicationWillTerminate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(FAST_APP_TERMINATE_ENABLED)

// Constants for deferring memory debugging tools startup.
NSString* const kMemoryDebuggingToolsStartup = @"MemoryDebuggingToolsStartup";

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

// Constants for deferring the enterprise managed device check.
NSString* const kEnterpriseManagedDeviceCheck = @"EnterpriseManagedDeviceCheck";

// Constants for deferred deletion of leftover session state files.
NSString* const kPurgeWebSessionStates = @"PurgeWebSessionStates";

// Constant for deffered memory experimentation.
NSString* const kMemoryExperimentation = @"BeginMemoryExperimentation";

// Constant for deferred automatic download deletion.
NSString* const kAutoDeletionFileRemoval = @"AutoDeletionFileRemoval";

// Constant for deferred default browser status API check.
NSString* const kDefaultBrowserStatusCheck = @"DefaultBrowserStatusCheck";

// Constant for enabling widgets for multi-profile.
NSString* const kWidgetsForMultiprofileKey = @"WidgetsForMultiprofileKey";

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

// Inserts `session_ids` into the set of discarded sessions for `attrs`.
void InsertDiscardedSessions(const std::set<std::string>& session_ids,
                             ProfileAttributesIOS& attrs) {
  auto discarded_sessions = attrs.GetDiscardedSessions();
  discarded_sessions.insert(session_ids.begin(), session_ids.end());
  attrs.SetDiscardedSessions(discarded_sessions);
}

// Mark all `sessions` as discarded sessions for all profiles.
void MarkSessionsAsDiscardedForAllProfiles(NSSet<UISceneSession*>* sessions) {
  ProfileAttributesStorageIOS* storage = GetApplicationContext()
                                             ->GetProfileManager()
                                             ->GetProfileAttributesStorage();

  // Prior to M-133, the list of sessions to discard was stored in a plist.
  // If the file still exists, then copy the session identifiers, and then
  // delete the file.
  std::set<std::string> sessionIDs =
      sessions_storage_util::GetDiscardedSessions();

  // Usually Chrome uses -[SceneState sceneSessionID] as identifier to properly
  // support devices that do not support multi-window (and which use a constant
  // identifier). For devices that do not support multi-window the session is
  // saved at a constant path, so it is harmless to delete files at a path
  // derived from -persistentIdentifier (since there won't be files deleted).
  // For devices that do support multi-window, there is data to delete once the
  // session is garbage collected.
  //
  // Thus it is always correct to use -persistentIdentifier here.
  for (UISceneSession* session in sessions) {
    sessionIDs.insert(base::SysNSStringToUTF8(session.persistentIdentifier));
  }

  storage->IterateOverProfileAttributes(
      base::BindRepeating(&InsertDiscardedSessions, sessionIDs));

  sessions_storage_util::ResetDiscardedSessions();
}

// It was found that -application:didDiscardSceneSessions: may be called with
// UISceneSession* corresponding to SceneState* that are still connected. It
// caused flakyness of EarlGrey tests (see https://crbug.com/390108895). The
// behaviour has only been confirmed for EarlGrey tests. Record an histogram
// counting how many Scenes are discarded while still connected to detect if
// the issue also reproduce in production (if it were to reproduce, it would
// cause unexplained tab losses).
//
// See https://crbug.com/392575873 for details.
void RecordDiscardSceneStillConnected(NSSet<UISceneSession*>* scene_sessions,
                                      NSArray<SceneState*>* connected_scenes) {
  // iPhone do not use -persistentIdentifier to identify the session data
  // for a SceneState, so they will never delete data. Only record metric
  // for iPad since even if the issue reproduce on iPhone, it won't have
  // any impact.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  NSUInteger count_discarded_scene_still_connected = 0;
  NSMutableSet<NSString*>* connected_identifiers = [[NSMutableSet alloc] init];
  for (SceneState* scene_state in connected_scenes) {
    [connected_identifiers
        addObject:base::SysUTF8ToNSString(scene_state.sceneSessionID)];
  }

  for (UISceneSession* scene_session in scene_sessions) {
    NSString* persistent_identifier = scene_session.persistentIdentifier;
    if ([connected_identifiers containsObject:persistent_identifier]) {
      ++count_discarded_scene_still_connected;
    }
  }

  base::UmaHistogramExactLinear(
      "IOS.Sessions.DiscardedScenesStillConnectedCount",
      count_discarded_scene_still_connected, 100);
}

// Possible choices for which profile to use for a scene.
enum class ProfileChoice {
  kProfileForScene,
  kProfileFromActivity,
  kLastUsedProfile,
  kPersonalProfile,
  kNewProfile,
};

// Returns the available ProfileChoices depending on the enabled features.
base::span<const ProfileChoice> GetProfileChoices() {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    static constexpr ProfileChoice kProfileChoicesWithSeparateAccounts[] = {
        ProfileChoice::kProfileForScene, ProfileChoice::kProfileFromActivity,
        ProfileChoice::kLastUsedProfile, ProfileChoice::kPersonalProfile,
        ProfileChoice::kNewProfile,
    };
    return kProfileChoicesWithSeparateAccounts;
  }

  static constexpr ProfileChoice kProfileChoices[] = {
      ProfileChoice::kProfileFromActivity,
      ProfileChoice::kPersonalProfile,
      ProfileChoice::kNewProfile,
  };
  return kProfileChoices;
}

// Returns the name of the profile for `choice`. May be empty in some cases,
// e.g. when a corresponding pref isn't set yet.
std::string GetProfileNameForChoice(ProfileChoice choice,
                                    SceneState* scene_state,
                                    ProfileManagerIOS* manager,
                                    ProfileAttributesStorageIOS* storage,
                                    PrefService* local_state) {
  switch (choice) {
    case ProfileChoice::kProfileFromActivity: {
      for (NSUserActivity* activity in scene_state.connectionOptions
               .userActivities) {
        std::string profile_name = GetProfileNameFromActivity(activity);
        if (!profile_name.empty()) {
          return profile_name;
        }
      }
      return std::string();
    }
    case ProfileChoice::kProfileForScene:
      return storage->GetProfileNameForSceneID(scene_state.sceneSessionID);
    case ProfileChoice::kLastUsedProfile:
      return local_state->GetString(prefs::kLastUsedProfile);
    case ProfileChoice::kPersonalProfile:
      return storage->GetPersonalProfileName();
    case ProfileChoice::kNewProfile:
      return manager->ReserveNewProfileName();
  }
  NOTREACHED();
}

}  // namespace

@interface MainController () <AppStateObserver,
                              BlockingSceneCommands,
                              ChangeProfileCommands,
                              PrefObserverDelegate,
                              ProfileStateObserver>

// Handles collecting metrics on user triggered screenshots
@property(nonatomic, strong)
    ScreenshotMetricsRecorder* screenshotMetricsRecorder;
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
// Schedules the removal of files that were scheduled for automatic deletion and
// were downloaded more than 30 days ago.
- (void)scheduleAutoDeletionFileRemoval;
// Crashes the application if requested.
- (void)crashIfRequested;
// Initializes the application to the minimum initialization needed in all
// cases.
- (void)startUpBrowserBasicInitialization;
// Initializes the browser objects for the background handlers, perform any
// background initilisation that are required, and then transition to the
// next stage.
- (void)startUpBrowserBackgroundInitialization;

@end

@implementation MainController {
  // The object that drives the Chrome startup/shutdown logic.
  std::unique_ptr<IOSChromeMain> _chromeMain;

  // True if the current session began from a cold start. False if the app has
  // entered the background at least once since start up.
  BOOL _isColdStart;

  // True if the launch metrics have already been recorded.
  BOOL _launchMetricsRecorded;

  // YES if the user has ever interacted with the application. May be NO if the
  // application has been woken up by the system for background work.
  BOOL _userInteracted;

  // Whether the application is currently in the background. Workaround for
  // rdar://22392526 where -applicationDidEnterBackground: can be called twice.
  // TODO(crbug.com/41211311): remove when rdar:22392526 is fixed
  BOOL _applicationInBackground;

  // YES if any Profile had initialized the UI for its first Scene.
  BOOL _firstWindowCreated;

  // An object to record metrics related to the user's first action.
  std::unique_ptr<FirstUserActionRecorder> _firstUserActionRecorder;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _localStatePrefObserverBridge;

  // Registrar for pref changes notifications to the local state.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // The class in charge of showing/hiding the memory debugger when the
  // appropriate pref changes.
  MemoryDebuggerManager* _memoryDebuggerManager;

  // Variable backing metricsMediator property.
  __weak MetricsMediator* _metricsMediator;

  // Holds the ProfileController for all loaded profiles.
  std::map<std::string, ProfileController*, std::less<>> _profileControllers;

  WindowConfigurationRecorder* _windowConfigurationRecorder;

  // Handler for the startup tasks, deferred or not.
  StartupTasks* _startupTasks;

  // The set of "scene sessions" that needs to be discarded. See
  // -application:didDiscardSceneSessions: for details.
  NSMutableSet<UISceneSession*>* _sceneSessionsToDiscard;

  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // The highest ProfileInitStage reached by any ProfileState. This value
  // can only be increased, never decreased. It gates application-level
  // initialisation that should only happen once at least one Profile has
  // reached a significant stage (e.g. loaded the session and allowed the
  // user to interact with the application, ...).
  ProfileInitStage _highestProfileInitStageReached;

  // Timer used to schedule the unload of unused profiles during the next
  // run loop (to avoid unloading a profile and destroying all objects in
  // an observer method as this can be dangerous if it destroy the sender).
  base::OneShotTimer _timer;
}

// Defined by public protocols.
// - StartupInformation
@synthesize isColdStart = _isColdStart;
@synthesize appLaunchTime = _appLaunchTime;
@synthesize isFirstRun = _isFirstRun;
@synthesize isTerminating = _isTerminating;
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
  NSBundle* baseBundle = base::apple::OuterBundle();
  base::apple::SetBaseBundleIDOverride(
      base::SysNSStringToUTF8(baseBundle.bundleIdentifier));

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

  // Register the ChangeProfileCommands handler with AccountProfileMapper.
  GetApplicationContext()
      ->GetAccountProfileMapper()
      ->SetChangeProfileCommandsHandler(HandlerForProtocol(
          self.appState.appCommandDispatcher, ChangeProfileCommands));

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
  if (_userInteracted) {
    GetApplicationContext()->GetMetricsService()->OnAppEnterForeground();
  }

  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());

  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];

  if (_sceneSessionsToDiscard) {
    [self application:[UIApplication sharedApplication]
        didDiscardSceneSessions:std::exchange(_sceneSessionsToDiscard, nil)];
  }

  [self.appState queueTransitionToNextInitStage];
}

// This initialization must happen before any windows are created.
- (void)startUpBeforeFirstWindowCreated {
  // TODO(crbug.com/40190949): Determine whether Chrome needs to resume
  // watching for crashes.
  self.appState.postCrashAction = [self postCrashAction];
  base::UmaHistogramEnumeration("Stability.IOS.PostCrashAction",
                                self.appState.postCrashAction);

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

  ios::provider::InstallOverrides();

  [self scheduleLowPriorityStartupTasks];

  // Now that everything is properly set up, run the tests.
  tests_hook::RunTestsIfPresent();

  self.screenshotMetricsRecorder = [[ScreenshotMetricsRecorder alloc] init];
  [self.screenshotMetricsRecorder startRecordingMetrics];
}

- (PostCrashAction)postCrashAction {
  if (self.appState.resumingFromSafeMode) {
    return PostCrashAction::kShowSafeMode;
  }

  if (GetApplicationContext()->WasLastShutdownClean()) {
    return PostCrashAction::kRestoreTabsCleanShutdown;
  }

  if (crash_util::GetFailedStartupAttemptCount() >= 2) {
    return PostCrashAction::kShowNTPWithReturnToTab;
  }

  return PostCrashAction::kRestoreTabsUncleanShutdown;
}

#pragma mark - AppLifetimeObserver

- (void)applicationWillResignActive:(UIApplication*)application {
  if (_appState.initStage < AppInitStage::kSafeMode) {
    return;
  }

  // Reset the failed startup count as the user was able to background the app.
  crash_util::ResetFailedStartupAttemptCount();

  // Nothing to do if no profile has been fully yet loaded.
  if (_highestProfileInitStageReached < ProfileInitStage::kPrepareUI) {
    return;
  }

  // -applicationWillResignActive: is called by the OS when the application
  // loses the focus (either because the last window is backgrounded or when
  // the user switch to another application while in split screen mode on an
  // iPad). Next time a windows become active, it should be considered as a
  // "cold start" since the application was still running without the focus
  // as opposed to a "warm start" which happens when the application was not
  // running and did the whole startup sequence before activating the window.
  _isColdStart = NO;

  // Forward the event to all ProfileControllers.
  for (const auto& pair : _profileControllers) {
    ProfileController* controller = pair.second;
    [controller applicationWillResignActive:application];
  }
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // Avoid re-entrancy, and then mark the app as terminating.
  CHECK(!_isTerminating);
  _isTerminating = YES;

  if (!_applicationInBackground) {
    base::UmaHistogramBoolean(
        "Stability.IOS.UTE.AppWillTerminateWasCalledInForeground", true);
  }

  [_appState.appCommandDispatcher prepareForShutdown];

  // Cancel any in-flight distribution notification.
  ios::provider::CancelAppDistributionNotifications();

  // Forward the event to all ProfileControllers.
  for (const auto& pair : _profileControllers) {
    ProfileController* controller = pair.second;
    [controller applicationWillTerminate:application];
  }

  [self stopChromeMain];
}

- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Exit the application if backgrounding while in safe mode.
  if (_appState.initStage == AppInitStage::kSafeMode) {
    exit(0);
  }

  if (_applicationInBackground) {
    return;
  }

  _applicationInBackground = YES;
  crash_keys::SetCurrentlyInBackground(true);

  // Reset `-startupHadExternalIntent` for all scenes in case external intents
  // were triggered while the application was in the foreground.
  for (SceneState* scene in _appState.connectedScenes) {
    scene.startupHadExternalIntent = NO;
  }

  // The remainder of the cleanup is only valid if at least one of the profile
  // has been initialized, so return early if this is not the case.
  if (_highestProfileInitStageReached < ProfileInitStage::kPrepareUI) {
    return;
  }

  [self expireFirstUserActionRecorder];

  // Forward the event to all ProfileControllers.
  for (const auto& pair : _profileControllers) {
    ProfileController* controller = pair.second;
    [controller applicationDidEnterBackground:application
                                 memoryHelper:memoryHelper];
  }

  // Mark the startup as clean if it hasn't already been.
  [_appState.deferredRunner runBlockNamed:kStartupResetAttemptCount];

  // Update metrics.
  [MetricsMediator logDateInUserDefaults];
  [MetricsMediator
      applicationDidEnterBackground:[memoryHelper
                                        foregroundMemoryWarningCount]];

  // Clear the memory warning flag since the application is in the background.
  PreviousSessionInfo* sessionInfo = [PreviousSessionInfo sharedInstance];
  [sessionInfo resetMemoryWarningFlag];
  [sessionInfo stopRecordingMemoryFootprint];

  GetApplicationContext()->OnAppEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application
                          memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Invariant: the application has passed AppInitStage::kStart.
  CHECK_GT(_appState.initStage, AppInitStage::kStart);

  // Fully initialize the browser objects for the browser UI if it is not
  // already the case. This is especially needed for scene startup.
  if (_highestProfileInitStageReached < ProfileInitStage::kPrepareUI) {
    // TODO(crbug.com/40760092): This function should only be called once
    // during a specific stage, but this requires non-trivial refactoring, so
    // for now #initializeUIPreSafeMode will just return early if called more
    // than once.
    // The application has been launched in background and the initialization
    // is not complete.
    [self initializeUIPreSafeMode];
    return;
  }

  // Don't go further with foregrounding the app when the app has not passed
  // safe mode yet or was initialized from the background.
  if (_appState.initStage <= AppInitStage::kSafeMode ||
      !_applicationInBackground) {
    return;
  }

  _applicationInBackground = NO;
  crash_keys::SetCurrentlyInBackground(false);

  GetApplicationContext()->OnAppEnterForeground();

  // Update the state of metrics and crash reporting as the method of
  // communication may have changed while the app was in the backgroumd.
  [_metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self
                             connectedScenes:_appState.connectedScenes];

  // Send any feedback that might still be on temporary storage.
  if (ios::provider::IsUserFeedbackSupported()) {
    ios::provider::UploadAllPendingUserFeedback();
  }

  // Forward the event to all ProfileControllers.
  for (const auto& pair : _profileControllers) {
    ProfileController* controller = pair.second;
    [controller applicationWillEnterForeground:application
                                  memoryHelper:memoryHelper];
  }

  base::RecordAction(base::UserMetricsAction("MobileWillEnterForeground"));

  // This will be a no-op if upload already started.
  crash_helper::UploadCrashReports();
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
  // This method is invoked by iOS to inform the application that the sessions
  // for "closed windows" are garbage collected and that any data associated
  // with them by the application needs to be deleted.
  //
  // The documentation says that if the application is not running when the OS
  // decides to discard the sessions, then it will call this method the next
  // time the application starts up. As seen by crbug.com/1292641, this call
  // happens before -[UIApplicationDelegate sceneWillConnect:] which means
  // that it can happen before Chrome has properly initialized. In that case,
  // record the list of sessions to discard and clean them once Chrome is
  // initialized.
  ApplicationContext* applicationContext = GetApplicationContext();
  if (!applicationContext) {
    if (!_sceneSessionsToDiscard) {
      _sceneSessionsToDiscard = [sceneSessions mutableCopy];
    } else {
      [_sceneSessionsToDiscard unionSet:sceneSessions];
    }
    return;
  }

  DCHECK_GE(_appState.initStage,
            AppInitStage::kBrowserObjectsForBackgroundHandlers);

  applicationContext->GetSystemIdentityManager()
      ->ApplicationDidDiscardSceneSessions(sceneSessions);

  MarkSessionsAsDiscardedForAllProfiles(sceneSessions);
  RecordDiscardSceneStillConnected(sceneSessions, _appState.connectedScenes);

  crash_keys::SetConnectedScenesCount(_appState.connectedScenes.count);
}

#pragma mark Early launch

// This method is the first to be called when user launches the application.
// This performs the minimal amount of browser initialization that is needed by
// safe mode.
// Depending on the background tasks history, the state of the application is
// INITIALIZATION_STAGE_BACKGROUND so this
// step cannot be included in the `startUpBrowserToStage:` method.
- (void)initializeUIPreSafeMode {
  if (_userInteracted) {
    return;
  }

  _userInteracted = YES;
  [self saveLaunchDetailsToDefaults];
  [_appState queueTransitionToNextInitStage];
}

// Saves the current launch details to user defaults.
- (void)saveLaunchDetailsToDefaults {
  PreviousSessionInfo* sessionInfo = [PreviousSessionInfo sharedInstance];

  // Reset the failure count on first launch, increment it on other launches.
  if ([sessionInfo isFirstSessionAfterUpgrade]) {
    crash_util::ResetFailedStartupAttemptCount();
  } else {
    crash_util::IncrementFailedStartupAttemptCount(false);
  }

  // The startup failure count *must* be synchronized now, since the crashes it
  // is trying to count are during startup.
  // -[PreviousSessionInfo beginRecordingCurrentSession] calls `synchronize` on
  // the user defaults, so leverage that to prevent calling it twice.

  // Start recording info about this session.
  [sessionInfo beginRecordingCurrentSession];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  // Install a LaunchScreenViewController as root view for the newly connected
  // SceneState and make the window visible (but do not force it above all the
  // other windows by making it key window). This ensures that something will
  // be displayed during the blocking steps of the application and/or profile
  // initialisation (e.g. fetching variation seeds, load profiles' preferences,
  // migrating session storage, ...).
  UIWindow* window = sceneState.window;
  window.rootViewController = [[LaunchScreenViewController alloc] init];
  window.hidden = NO;

  if (appState.initStage < AppInitStage::kFinal) {
    return;
  }

  ApplicationContext* applicationContext = GetApplicationContext();
  ProfileManagerIOS* manager = applicationContext->GetProfileManager();
  ProfileAttributesStorageIOS* storage = manager->GetProfileAttributesStorage();
  PrefService* localState = applicationContext->GetLocalState();

  [self attachProfileToScene:sceneState
              profileManager:manager
           attributesStorage:storage
                  localState:localState];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
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
    case AppInitStage::kFinal:
      [self attachProfilesToAllConnectedScenes];
      break;
  }
}

- (void)addPostSafeModeAgents {
  [self.appState addAgent:[[EnterpriseAppAgent alloc] init]];
  [self.appState addAgent:[[IncognitoUsageAppStateAgent alloc] init]];
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  [self.appState addAgent:[[CredentialProviderMigratorAppAgent alloc] init]];
#endif
  [self.appState addAgent:[[DefaultBrowserBannerPromoAppAgent alloc] init]];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    sceneDisconnected:(SceneState*)sceneState {
  if (profileState.connectedScenes.count == 0) {
    [self scheduleDropUnusedProfileControllers];
  }
}

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage > _highestProfileInitStageReached) {
    switch (nextInitStage) {
      case ProfileInitStage::kStart:
        NOTREACHED();

      case ProfileInitStage::kLoadProfile:
      case ProfileInitStage::kMigrateStorage:
      case ProfileInitStage::kPurgeDiscardedSessionsData:
      case ProfileInitStage::kProfileLoaded:
      case ProfileInitStage::kPrepareUI:
        // Nothing to do.
        break;

      case ProfileInitStage::kUIReady:
        [self startUpBeforeFirstWindowCreated];
        break;

      case ProfileInitStage::kFirstRun:
      case ProfileInitStage::kChoiceScreen:
      case ProfileInitStage::kNormalUI:
      case ProfileInitStage::kFinal:
        // Nothing to do.
        break;
    }

    if (fromInitStage == ProfileInitStage::kFirstRun) {
      // Clear -isFirstRun once the first profile is done presenting the FRE
      // (it should not be presented if e.g. the user sign-in with a managed
      // profile on their first run of the app after completing the FRE on
      // the personal profile).
      _isFirstRun = NO;
    }
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage > _highestProfileInitStageReached) {
    _highestProfileInitStageReached = nextInitStage;
    switch (nextInitStage) {
      case ProfileInitStage::kStart:
        NOTREACHED();

      case ProfileInitStage::kLoadProfile:
      case ProfileInitStage::kMigrateStorage:
      case ProfileInitStage::kPurgeDiscardedSessionsData:
        // Nothing to do.
        break;

      case ProfileInitStage::kProfileLoaded:
        [self scheduleRLZInitWithProfile:profileState.profile];
        break;

      case ProfileInitStage::kPrepareUI:
      case ProfileInitStage::kUIReady:
        // Nothing to do.
        break;

      case ProfileInitStage::kFirstRun:
        // Stop forcing the orientation at the application level (if it was) as
        // the ProfileAgent are now responsible for forcing the orientation.
        _scopedForceOrientation.reset();
        break;

      case ProfileInitStage::kChoiceScreen:
      case ProfileInitStage::kNormalUI:
        // Nothing to do.
        break;

      case ProfileInitStage::kFinal:
        // Request the deletion of the data for all profiles marked for
        // deletion when the first profile is successfully loaded.
        GetApplicationContext()
            ->GetProfileManager()
            ->PurgeProfilesMarkedForDeletion(base::DoNothing());
        break;
    }
  }

  // This should happen for all ProfileStage as it is responsible for
  // recording the lauch metrics which should wait until all SceneStates
  // have been mapped to Profiles.
  if (nextInitStage == ProfileInitStage::kFinal) {
    [MetricsMediator logProfileLoadMetrics:profileState.profile];
    [self recordLaunchMetrics];
  }
}

// Called when the first scene becomes active.
- (void)profileState:(ProfileState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  if (_firstWindowCreated) {
    return;
  }

  _firstWindowCreated = YES;
  [self startUpAfterFirstWindowCreated];
}

#pragma mark - Property implementation.

- (void)setAppState:(AppState*)appState {
  DCHECK(!_appState);
  _appState = appState;
  [appState addObserver:self];

  // If this is the first run, force the portrait orientation on iPhone at
  // the application level (until at least one ProfileController reaches
  // the kFirstRun stage).
  //
  // This is because the FRE is designed to only be displayed in portrait
  // orientation on iPhone but the FRE happen as part of the profile init
  // and waiting until then to force the orientation introduces unpleasant
  // animation.
  //
  // There may be some unpleasant animation if other screen want to force
  // the orientation (such as the search engine choice screen) as it may
  // not be possible to determine here whether they will be run (e.g. if
  // the depend on the state of a profile).
  if (_isFirstRun) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(_appState);
  }

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
    [appState
        addAgent:[[WindowAccessibilityChangeNotifierAppAgent alloc] init]];
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

#if BUILDFLAG(ENABLE_RLZ)
  rlz::RLZTracker::CleanupRlz();
#endif

  _chromeMain.reset();
}

#pragma mark - Startup tasks

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
  NSNumber* supportsShowDefaultBrowserPromo = @YES;

  NSMutableDictionary* capabilities = [[NSMutableDictionary alloc] init];
  [capabilities setObject:supportsShowDefaultBrowserPromo
                   forKey:app_group::kChromeShowDefaultBrowserPromoCapability];

  if (base::FeatureList::IsEnabled(kYoutubeIncognito) &&
      base::FeatureList::IsEnabled(kChromeStartupParametersAsync)) {
    [capabilities
        setObject:@[ app_group::kYoutubeBundleID ]
           forKey:app_group::kChromeSupportOpenLinksParametersFromCapability];
  } else {
    [capabilities
        removeObjectForKey:app_group::
                               kChromeSupportOpenLinksParametersFromCapability];
  }

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
    kWidgetsForMultiprofileKey : @{
      kFieldTrialValueKey : @(IsWidgetsForMultiprofileEnabled()),
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
  [self sendQueuedFeedback];
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
  [self scheduleAutoDeletionFileRemoval];
  [self scheduleDefaultBrowserStatusCheck];
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

- (void)scheduleAutoDeletionFileRemoval {
  __weak StartupTasks* startupTasks = _startupTasks;
  [_appState.deferredRunner
      enqueueBlockNamed:kAutoDeletionFileRemoval
                  block:^{
                    [startupTasks removeFilesScheduledForAutoDeletion];
                  }];
}

- (void)scheduleDefaultBrowserStatusCheck {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_appState.deferredRunner
      enqueueBlockNamed:kDefaultBrowserStatusCheck
                  block:^{
                    default_status::TriggerDefaultStatusCheck();
                  }];
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
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
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (expireFirstUserActionRecorder)
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

// Schedules the initialization of the RLZ library with a give profile. This
// method must only be called once. If this is the first run of the app, it
// will record the installation event.
- (void)scheduleRLZInitWithProfile:(ProfileIOS*)profile {
#if BUILDFLAG(ENABLE_RLZ)
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();

  // Negative delay means to send ping immediately after first recorded search.
  const int pingDelay = prefs->GetInteger(FirstRun::GetPingDelayPrefName());
  rlz::RLZTracker::SetRlzDelegate(std::make_unique<RLZTrackerDelegateImpl>());
  rlz::RLZTracker::InitRlzDelayed(
      FirstRun::IsChromeFirstRun(), pingDelay < 0,
      base::Milliseconds(abs(pingDelay)),
      RLZTrackerDelegateImpl::IsGoogleDefaultSearch(profile),
      RLZTrackerDelegateImpl::IsGoogleHomepage(profile),
      RLZTrackerDelegateImpl::IsGoogleInStartpages(profile));
#endif
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

- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  CHECK_EQ(self.appState.initStage, AppInitStage::kFinal);

  CHECK(sceneState);
  CHECK([self.appState.connectedScenes containsObject:sceneState]);

  base::UmaHistogramEnumeration("Signin.IOSChangeProfileReason", reason);

  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();
  CHECK(manager->HasProfileWithName(profileName));

  // Get the SceneDelegate from the SceneState.
  UIWindowScene* scene = sceneState.scene;
  SceneDelegate* sceneDelegate =
      base::apple::ObjCCast<SceneDelegate>(scene.delegate);
  CHECK(sceneDelegate);

  ChangeProfileAnimator* animator = [[ChangeProfileAnimator alloc]
      initWithWindow:base::apple::ObjCCast<ChromeOverlayWindow>(
                         sceneDelegate.window)];

  ProfileAttributesStorageIOS* storage = manager->GetProfileAttributesStorage();
  const std::string& sceneIdentifier = sceneState.sceneSessionID;

  // If the SceneState is not associated with the correct profile, then
  // perform the necessary work to switch the profile used for the scene.
  if (profileName != storage->GetProfileNameForSceneID(sceneIdentifier)) {
    // The UI has to be destroyed, start animating.
    [animator startAnimation];

    // Set the mapping between profile and scene.
    storage->SetProfileNameForSceneID(sceneIdentifier, profileName);

    // Pretend the scene has been disconnected, then reconnect it.
    const SceneActivationLevel savedLevel = sceneState.activationLevel;
    UISceneConnectionOptions* savedOptions = sceneState.connectionOptions;

    // Destroy the old SceneState and recreate it.
    [sceneDelegate sceneDidDisconnect:scene];
    [sceneDelegate scene:scene
        willConnectToSession:scene.session
                     options:savedOptions];

    sceneState = sceneDelegate.sceneState;
    DCHECK(sceneState);

    // Reconnect the scene. This will attach a profile automatically based
    // on the information stored in the ProfileAttributesStorageIOS.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UISceneWillConnectNotification
                      object:scene];
    DCHECK(sceneState.profileState);

    while (sceneState.activationLevel < savedLevel) {
      sceneState.activationLevel = static_cast<SceneActivationLevel>(
          base::to_underlying(sceneState.activationLevel) + 1);
    }
  }

  // Wait for the profile to complete its initialisation.
  [animator waitForSceneState:sceneState
             toReachInitStage:ProfileInitStage::kNormalUI
                 continuation:std::move(continuation)];
}

- (void)deleteProfile:(std::string_view)profileName {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  CHECK_EQ(self.appState.initStage, AppInitStage::kFinal);
  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();
  CHECK(manager->CanDeleteProfileWithName(profileName));
  const std::string& personalProfile =
      manager->GetProfileAttributesStorage()->GetPersonalProfileName();
  DCHECK_GT(personalProfile.size(), 0u);

  // Mark the profile for deletion. If there is no UI attached for the
  // profile, there is nothing else to do (it may be loaded by another
  // part of the code, and will be unloaded when no longer used).
  manager->MarkProfileForDeletion(profileName);
  auto iter = _profileControllers.find(profileName);
  if (iter == _profileControllers.end()) {
    return;
  }

  ProfileController* controller = iter->second;
  NSArray<SceneState*>* scenes = controller.state.connectedScenes;

  // If there are no connected scenes, then there is no need to change
  // the profile for the scene. Do not immediately drop the profile as
  // there may still be objects that are shutting down. Schedules a
  // call to -dropUnusedProfileControllers to drop it in the next loop.
  if (scenes.count == 0) {
    [self scheduleDropUnusedProfileControllers];
    return;
  }

  // Otherwise, change the profile for all connected scenes. This will
  // result in a call to -profileState:sceneDisconnected: for each one
  // and eventually a call to -scheduleUnloadUnusedProfiles.
  auto continuation = base::BindRepeating(
      [](SceneState* scene_state, base::OnceClosure closure) {
        std::move(closure).Run();
      });

  for (SceneState* scene in scenes) {
    [self changeProfile:personalProfile
               forScene:scene
                 reason:ChangeProfileReason::kProfileDeleted
           continuation:continuation];
  }
}

#pragma mark - Private

// Attach a Profile to all connected scenes.
- (void)attachProfilesToAllConnectedScenes {
  ApplicationContext* applicationContext = GetApplicationContext();
  ProfileManagerIOS* manager = applicationContext->GetProfileManager();
  ProfileAttributesStorageIOS* storage = manager->GetProfileAttributesStorage();
  PrefService* localState = applicationContext->GetLocalState();

  for (SceneState* sceneState in self.appState.connectedScenes) {
    [self attachProfileToScene:sceneState
                profileManager:manager
             attributesStorage:storage
                    localState:localState];
  }
}

// Attach a profile to `sceneState`.
- (void)attachProfileToScene:(SceneState*)sceneState
              profileManager:(ProfileManagerIOS*)manager
           attributesStorage:(ProfileAttributesStorageIOS*)storage
                  localState:(PrefService*)localState {
  const std::string& sceneID = sceneState.sceneSessionID;

  // Determine which profile to use. The logic is to take the first valid
  // profile (i.e. the value is set and the profile is known) amongst the
  // following value: the profile configured for the scene, the last used
  // profile, the personal profile, or as a last resort a new profile.
  std::string profileName;
  for (ProfileChoice choice : GetProfileChoices()) {
    profileName = GetProfileNameForChoice(choice, sceneState, manager, storage,
                                          localState);

    // Pick the first valid profile name found.
    if (storage->HasProfileWithName(profileName)) {
      break;
    }
  }

  // A valid profile name must have been picked (in the last resort a
  // new profile name must have been generated).
  CHECK(storage->HasProfileWithName(profileName));

  // If the mapping has changed, store the mapping between the SceneID
  // and the profile in the ProfileAttributesStorageIOS so that it is
  // accessible the next time the window is open.
  if (profileName != storage->GetProfileNameForSceneID(sceneID)) {
    storage->SetProfileNameForSceneID(sceneID, profileName);
  }

  // Update kLastUsedProfile, to ensure that new window will use the same
  // profile (this also make sure that opening a second window will not
  // create a profile for users upgrading from M-132 or ealier where the
  // kLastUsedProfile was sometimes not correctly updated).
  localState->SetString(prefs::kLastUsedProfile, profileName);

  auto iterator = _profileControllers.find(profileName);
  if (iterator == _profileControllers.end()) {
    ProfileController* controller =
        [[ProfileController alloc] initWithAppState:self.appState
                                    metricsMediator:_metricsMediator];
    [controller.state addObserver:self];

    auto insertion_result =
        _profileControllers.insert(std::make_pair(profileName, controller));
    DCHECK(insertion_result.second);
    iterator = insertion_result.first;

    // Start loading the profile.
    [controller loadProfileNamed:profileName usingManager:manager];
  }

  DCHECK(iterator != _profileControllers.end());
  ProfileState* state = iterator->second.state;
  DCHECK(state != nil);

  // Attach the SceneState to the ProfileState.
  [sceneState.controller setProfileState:state];
}

// Drops all unused profile controllers. This will cause the corresponding
// Profile to be unloaded unless another code keep them alive.
- (void)dropUnusedProfileControllers {
  std::vector<std::string> profilesToUnload;
  for (const auto& [name, controller] : _profileControllers) {
    if (controller.state.connectedScenes.count == 0) {
      profilesToUnload.push_back(name);
    }
  }

  if (profilesToUnload.empty()) {
    return;
  }

  for (const auto& name : profilesToUnload) {
    auto iter = _profileControllers.find(name);
    CHECK(iter != _profileControllers.end());

    ProfileController* controller = iter->second;
    CHECK_EQ(controller.state.connectedScenes.count, 0u);
    [controller.state removeObserver:self];

    // Call -shutdown before deleting the object. This will unload the
    // profile if the keep alive refcount reaches zero.
    [controller shutdown];
    _profileControllers.erase(iter);
  }

  [self updateLastUsedProfilePref];
}

// Update the kLastUsedProfile preference if needed.
- (void)updateLastUsedProfilePref {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (base::Contains(_profileControllers,
                     localState->GetString(prefs::kLastUsedProfile))) {
    // The last used profile is still loaded, no need to update the pref.
    return;
  }

  // Find the name of the profile which most recently had a scene connected.
  std::string mostRecentlyUsedProfile;
  base::TimeTicks lastSceneConnection = base::TimeTicks::Min();
  for (const auto& [name, controller] : _profileControllers) {
    const base::TimeTicks timestamp = controller.state.lastSceneConnection;
    if (timestamp > lastSceneConnection) {
      lastSceneConnection = timestamp;
      mostRecentlyUsedProfile = name;
    }
  }

  // If mostRecentlyUsedProfile is empty, then there is no profile connected,
  // which usually mean that app will shutdown. In that case, do not update
  // the preference.
  if (!mostRecentlyUsedProfile.empty()) {
    localState->SetString(prefs::kLastUsedProfile, mostRecentlyUsedProfile);
  }
}

// Schedule a call to -dropUnusedProfileControllers at the next run loop.
- (void)scheduleDropUnusedProfileControllers {
  if (!_timer.IsRunning()) {
    __weak __typeof(self) weakSelf = self;
    _timer.Start(FROM_HERE, base::Seconds(0), base::BindOnce(^{
                   [weakSelf dropUnusedProfileControllers];
                 }));
  }
}

@end
