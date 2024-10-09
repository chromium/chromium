// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/barrier_closure.h"
#import "base/critical_closure.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/metrics/metrics_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/metrics/model/web_state_list_metrics_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"
#import "ui/base/device_form_factor.h"

namespace {
NSString* const kStartupAttemptReset = @"StartupAttemptReset";

// Flushes the CookieStore on the IO thread and invoke `closure` upon
// completion. The sequence where `closure` is invoked is unspecified.
void FlushCookieStoreOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  getter->GetURLRequestContext()->cookie_store()->FlushStore(
      std::move(closure));
}
}  // namespace

#pragma mark - AppStateObserverList

@interface AppStateObserverList : CRBProtocolObservers <AppStateObserver>
@end

@implementation AppStateObserverList
@end

#pragma mark - AppStateObserverList

@interface UIBlockerManagerObserverList
    : CRBProtocolObservers <UIBlockerManagerObserver>
@end

@implementation UIBlockerManagerObserverList
@end

#pragma mark - AppState

@interface AppState () <AppStateObserver>

// Container for observers.
@property(nonatomic, strong) AppStateObserverList* observers;

// Container for observers.
@property(nonatomic, strong)
    UIBlockerManagerObserverList* uiBlockerManagerObservers;

// YES if cookies are currently being flushed to disk. Declared as a property
// to allow modifying it in a block via a __weak pointer without checking if
// the pointer is nil or not.
@property(nonatomic, assign) BOOL savingCookies;

// This method is the first to be called when user launches the application.
// This performs the minimal amount of browser initialization that is needed by
// safe mode.
// Depending on the background tasks history, the state of the application is
// INITIALIZATION_STAGE_BACKGROUND so this
// step cannot be included in the `startUpBrowserToStage:` method.
- (void)initializeUIPreSafeMode;

// Complete the browser initialization for a regular startup.
- (void)completeUIInitialization;

// Saves the current launch details to user defaults.
- (void)saveLaunchDetailsToDefaults;

// Redefined as readwrite.
@property(nonatomic, assign) BOOL firstSceneHasInitializedUI;

// The current blocker target if any.
@property(nonatomic, weak, readwrite) id<UIBlockerTarget> uiBlockerTarget;

// The counter of currently shown blocking UIs. Do not use this directly,
// instead use incrementBlockingUICounterForScene: and
// incrementBlockingUICounterForScene or the ScopedUIBlocker.
@property(nonatomic, assign) NSUInteger blockingUICounter;

// Agents attached to this app state.
@property(nonatomic, strong) NSMutableArray<id<AppStateAgent>>* agents;

// A flag that tracks if the init stage is currently being incremented. Used to
// prevent reentrant calls to queueTransitionToNextInitStage originating from
// stage change notifications.
@property(nonatomic, assign) BOOL isIncrementingInitStage;

// A flag that tracks if another increment of init stage needs to happen after
// this one is complete. Will be set if queueTransitionToNextInitStage is called
// while queueTransitionToNextInitStage is already on the call stack.
@property(nonatomic, assign) BOOL needsIncrementInitStage;

@end

@implementation AppState {
  // Whether the application is currently in the background.
  // This is a workaround for rdar://22392526 where
  // -applicationDidEnterBackground: can be called twice.
  // TODO(crbug.com/41211311): Remove this once rdar://22392526 is fixed.
  BOOL _applicationInBackground;

  // Counter of number of object that want to force the device in the
  // portrait orientation (orientation is locked if non-zero).
  NSUInteger _forcePortraitOrientationCounter;
}

@synthesize userInteracted = _userInteracted;

- (instancetype)initWithStartupInformation:
    (id<StartupInformation>)startupInformation {
  self = [super init];
  if (self) {
    _observers = [AppStateObserverList
        observersWithProtocol:@protocol(AppStateObserver)];
    _uiBlockerManagerObservers = [UIBlockerManagerObserverList
        observersWithProtocol:@protocol(UIBlockerManagerObserver)];
    _agents = [[NSMutableArray alloc] init];
    _startupInformation = startupInformation;
    _appCommandDispatcher = [[CommandDispatcher alloc] init];

    // Subscribe to scene connection notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(sceneWillConnect:)
               name:UISceneWillConnectNotification
             object:nil];

    // Observe the status of VoiceOver for crash logging.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverStatusDidChange:)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];
    crash_keys::SetVoiceOverRunning(UIAccessibilityIsVoiceOverRunning());

    [self addObserver:self];
  }
  return self;
}

#pragma mark - Properties implementation

- (void)setUiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget {
  _uiBlockerTarget = uiBlockerTarget;
  for (SceneState* scene in self.connectedScenes) {
    // When there's a scene with blocking UI, all other scenes should show the
    // overlay.
    BOOL shouldPresentOverlay =
        (uiBlockerTarget != nil) && (scene != uiBlockerTarget);
    scene.presentingModalOverlay = shouldPresentOverlay;
  }
}

// Do not use this setter directly, instead use -queueTransitionToInitStage:
// that provides reentry guards.
- (void)setInitStage:(AppInitStage)newInitStage {
  DCHECK_GE(newInitStage, AppInitStage::kStart);
  DCHECK_LE(newInitStage, AppInitStage::kFinal);
  // As of writing this, it seems reasonable for init stages to be strictly
  // incremented by one only: if a stage needs to be skipped, it can just be a
  // no-op, but the observers will get a chance to react to it normally. If in
  // the future these need to be skipped, or go backwards:
  // 1. Check that all observers will support this change
  // 2. Keep the previous init stage and modify addObserver: code to send the
  // previous init stage instead.
  if (newInitStage == AppInitStage::kStart) {
    DCHECK_EQ(_initStage, AppInitStage::kStart);
  } else {
    DCHECK_EQ(base::to_underlying(newInitStage),
              base::to_underlying(_initStage) + 1);
  }

  AppInitStage previousInitStage = _initStage;
  [self.observers appState:self willTransitionToInitStage:newInitStage];
  _initStage = newInitStage;
  [self.observers appState:self didTransitionFromInitStage:previousInitStage];
}

- (BOOL)portraitOnly {
  return _forcePortraitOrientationCounter > 0;
}

- (NSArray<id<AppStateAgent>>*)connectedAgents {
  return [self.agents copy];
}

#pragma mark - Public methods.

- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Exit the app if backgrounding the app while being in safe mode.
  if (self.initStage == AppInitStage::kSafeMode) {
    exit(0);
  }

  if (_applicationInBackground) {
    return;
  }
  _applicationInBackground = YES;

  crash_keys::SetCurrentlyInBackground(true);

  if (self.initStage < AppInitStage::kBrowserObjectsForUI) {
    // The clean-up done in `-applicationDidEnterBackground:` is only valid for
    // the case when the application is started in foreground, so there is
    // nothing to clean up as the application was not initialized for
    // foreground.
    //
    // From the stack trace of the crash bug http://crbug.com/437307 , it
    // seems that `-applicationDidEnterBackground:` may be called when the app
    // is started in background and before the initialization for background
    // stage is done. Note that the crash bug could not be reproduced though.
    return;
  }

  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    enterprise_idle::IdleServiceFactory::GetForProfile(profile)
        ->OnApplicationWillEnterBackground();
  }

  [MetricsMediator
      applicationDidEnterBackground:[memoryHelper
                                        foregroundMemoryWarningCount]];

  [self.startupInformation expireFirstUserActionRecorder];

  // TODO(crbug.com/325596562): Update this for multiple profiles and for
  // per-state cookie storage.
  if (!_savingCookies) {
    NSSet<ProfileState*>* profileStates = self.connectedProfileStates;
    if (profileStates.count != 0) {
      // Record that saving the cookies has started to prevent posting multiple
      // tasks if the user quickly background, foreground and background the app
      // again.
      _savingCookies = YES;

      // The closure may be called on any sequence, so ensure it is posted back
      // on the current one by using base::BindPostTask(). The critical closure
      // guarantees that the task will be run before backgrounding. The barrier
      // callback ensures that the operation is considered complete when all the
      // profile's cookies have been saved.
      __weak AppState* weakSelf = self;
      base::RepeatingClosure closure = base::BarrierClosure(
          profileStates.count,
          base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                             base::MakeCriticalClosure(
                                 "applicationDidEnterBackground:_savingCookies",
                                 base::BindOnce(^{
                                   weakSelf.savingCookies = NO;
                                 }),
                                 /*is_immediate=*/true)));

      for (ProfileState* profileState in profileStates) {
        // Saving the cookies needs to happen on the IO thread.
        web::GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &FlushCookieStoreOnIOThread,
                base::WrapRefCounted(profileState.profile->GetRequestContext()),
                closure));
      }
    }
  }

  // Mark the startup as clean if it hasn't already been.
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kStartupAttemptReset];
  // Set date/time that the background fetch handler was called in the user
  // defaults.
  [MetricsMediator logDateInUserDefaults];
  // Clear the memory warning flag since the app is now safely in background.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];
  [[PreviousSessionInfo sharedInstance] stopRecordingMemoryFootprint];

  GetApplicationContext()->OnAppEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Fully initialize the browser objects for the browser UI if it is not
  // already the case. This is especially needed for scene startup.
  if (self.initStage < AppInitStage::kBrowserObjectsForUI) {
    // Invariant: The app has passed AppInitStage::kStart.
    CHECK(self.initStage != AppInitStage::kStart);
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
  if (self.initStage <= AppInitStage::kSafeMode || !_applicationInBackground) {
    return;
  }

  _applicationInBackground = NO;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    AuthenticationServiceFactory::GetForProfile(profile)
        ->OnApplicationWillEnterForeground();

    enterprise_idle::IdleServiceFactory::GetForProfile(profile)
        ->OnApplicationWillEnterForeground();
  }

  crash_keys::SetCurrentlyInBackground(false);

  // Update the state of metrics and crash reporting, as the method of
  // communication may have changed while the app was in the background.
  [metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Send any feedback that might be still on temporary storage.
  if (ios::provider::IsUserFeedbackSupported()) {
    ios::provider::UploadAllPendingUserFeedback();
  }

  GetApplicationContext()->OnAppEnterForeground();

  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self.startupInformation
                             connectedScenes:self.connectedScenes];
  [memoryHelper resetForegroundMemoryWarningCount];

  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile);
    // Send the "Chrome Opened" event to the feature_engagement::Tracker on a
    // warm start.
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
    [metricsMediator notifyCredentialProviderWasUsed:tracker];
  }

  base::RecordAction(base::UserMetricsAction("MobileWillEnterForeground"));

  // This will be a no-op if upload already started.
  crash_helper::UploadCrashReports();
}

- (void)applicationWillTerminate:(UIApplication*)application {
  if (!_applicationInBackground) {
    base::UmaHistogramBoolean(
        "Stability.IOS.UTE.AppWillTerminateWasCalledInForeground", true);
  }
  if (_appIsTerminating) {
    // Previous handling of this method spun the runloop, resulting in
    // recursive calls; this does not appear to happen with the new shutdown
    // flow, but this is here to ensure that if it can happen, it gets noticed
    // and fixed.
    CHECK(false);
  }
  _appIsTerminating = YES;

  [_appCommandDispatcher prepareForShutdown];

  // Cancel any in-flight distribution notifications.
  ios::provider::CancelAppDistributionNotifications();

  // Halt the tabs, so any outstanding requests get cleaned up, without actually
  // closing the tabs. Set the BVC to inactive to cancel all the dialogs.
  // Don't do this if there are no scenes, since there's no defined interface
  // provider (and no tabs).
  if (self.initStage >= AppInitStage::kBrowserObjectsForUI) {
    for (SceneState* sceneState in self.connectedScenes) {
      Browser* browser =
          sceneState.browserProviderInterface.currentBrowserProvider.browser;
      if (browser && WebUsageEnablerBrowserAgent::FromBrowser(browser)) {
        WebUsageEnablerBrowserAgent::FromBrowser(browser)->SetWebUsageEnabled(
            false);
      }
    }
  }

  [self.startupInformation stopChromeMain];
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
  DCHECK_GE(self.initStage, AppInitStage::kBrowserObjectsForBackgroundHandlers);

  GetApplicationContext()
      ->GetSystemIdentityManager()
      ->ApplicationDidDiscardSceneSessions(sceneSessions);

  // Usually Chrome uses -[SceneState sceneSessionID] as identifier to properly
  // support devices that do not support multi-window (and which use a constant
  // identifier). For devices that do not support multi-window the session is
  // saved at a constant path, so it is harmless to delete files at a path
  // derived from -persistentIdentifier (since there won't be files deleted).
  // For devices that do support multi-window, there is data to delete once the
  // session is garbage collected.
  //
  // Thus it is always correct to use -persistentIdentifier here.
  std::set<std::string> sessionIDs;
  for (UISceneSession* session in sceneSessions) {
    sessionIDs.insert(base::SysNSStringToUTF8(session.persistentIdentifier));
  }
  sessions_storage_util::MarkSessionsForRemoval(std::move(sessionIDs));
  crash_keys::SetConnectedScenesCount([self connectedScenes].count);
}

- (void)willResignActive {
  // Regardless of app state, if the user is able to background the app, reset
  // the failed startup count.
  crash_util::ResetFailedStartupAttemptCount();

  if (self.initStage < AppInitStage::kBrowserObjectsForUI) {
    // If the application did not pass the foreground initialization stage,
    // there is no active tab model to resign.
    return;
  }

  // Set [self.startupInformation isColdStart] to NO in anticipation of the next
  // time the app becomes active.
  [self.startupInformation setIsColdStart:NO];

  // Record session metrics.
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    SessionMetrics::FromProfile(profile)->RecordAndClearSessionMetrics(
        MetricsToRecordFlags::kActivatedTabCount);

    if (profile->HasOffTheRecordProfile()) {
      ProfileIOS* otrProfile = profile->GetOffTheRecordProfile();
      SessionMetrics::FromProfile(otrProfile)
          ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);
    }
  }
}

- (void)addObserver:(id<AppStateObserver>)observer {
  [self.observers addObserver:observer];

  if ([observer respondsToSelector:@selector(appState:
                                       didTransitionFromInitStage:)] &&
      self.initStage > AppInitStage::kStart) {
    AppInitStage previousInitStage =
        static_cast<AppInitStage>(base::to_underlying(self.initStage) - 1);
    // Trigger an update on the newly added agent.
    [observer appState:self didTransitionFromInitStage:previousInitStage];
  }
}

- (void)removeObserver:(id<AppStateObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)addAgent:(id<AppStateAgent>)agent {
  DCHECK(agent);
  [self.agents addObject:agent];
  [agent setAppState:self];
}

- (void)removeAgent:(id<AppStateAgent>)agent {
  DCHECK(agent);
  DCHECK([self.agents containsObject:agent]);
  [self.agents removeObject:agent];
}

- (void)queueTransitionToNextInitStage {
  DCHECK_LT(self.initStage, AppInitStage::kFinal);
  AppInitStage nextInitStage =
      static_cast<AppInitStage>(base::to_underlying(self.initStage) + 1);
  [self queueTransitionToInitStage:nextInitStage];
}

- (void)startInitialization {
  [self queueTransitionToInitStage:AppInitStage::kStart];
}

#pragma mark - Multiwindow-related

- (SceneState*)foregroundActiveScene {
  for (SceneState* sceneState in self.connectedScenes) {
    if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
      return sceneState;
    }
  }

  return nil;
}

- (NSArray<SceneState*>*)connectedScenes {
  NSMutableArray* sceneStates = [[NSMutableArray alloc] init];
  NSSet* connectedScenes = [UIApplication sharedApplication].connectedScenes;
  for (UIWindowScene* scene in connectedScenes) {
    if (![scene.delegate isKindOfClass:[SceneDelegate class]]) {
      // This might happen in tests.
      // TODO(crbug.com/40710078): This shouldn't be needed. (It might also
      // be the cause of crbug.com/1142782).
      [sceneStates addObject:[[SceneState alloc] initWithAppState:self]];
      continue;
    }

    SceneDelegate* sceneDelegate =
        base::apple::ObjCCastStrict<SceneDelegate>(scene.delegate);
    [sceneStates addObject:sceneDelegate.sceneState];
  }
  return sceneStates;
}

- (NSArray<SceneState*>*)foregroundScenes {
  return [self.connectedScenes
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   SceneState* scene,
                                                   NSDictionary* bindings) {
        return scene.activationLevel >= SceneActivationLevelForegroundInactive;
      }]];
}

- (void)initializeUIPreSafeMode {
  // TODO(crbug.com/40760092): Consider replacing this with a DCHECK once we
  // make sure that #initializeUIPreSafeMode is only called once. This should
  // be done in a one-line change that is easy to revert.
  // Only perform the pre-safemode initialization once.
  if (_userInteracted) {
    return;
  }

  _userInteracted = YES;
  [self saveLaunchDetailsToDefaults];

  // Continue the initialization.
  [self queueTransitionToNextInitStage];
}

- (void)completeUIInitialization {
  DCHECK([self.startupInformation isColdStart]);
}

#pragma mark - Internal methods.

- (void)saveLaunchDetailsToDefaults {
  // Reset the failure count on first launch, increment it on other launches.
  if ([[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade])
    crash_util::ResetFailedStartupAttemptCount();
  else
    crash_util::IncrementFailedStartupAttemptCount(false);

  // The startup failure count *must* be synchronized now, since the crashes it
  // is trying to count are during startup.
  // -[PreviousSessionInfo beginRecordingCurrentSession] calls `synchronize` on
  // the user defaults, so leverage that to prevent calling it twice.

  // Start recording info about this session.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
}

- (void)queueTransitionToInitStage:(AppInitStage)initStage {
  if (self.isIncrementingInitStage) {
    // It is an error to queue more than one transition at once.
    DCHECK(!self.needsIncrementInitStage);

    // Set a flag to increment after the observers are notified of the current
    // change.
    self.needsIncrementInitStage = YES;
    return;
  }

  self.isIncrementingInitStage = YES;
  self.initStage = initStage;
  // TODO(crbug.com/353683675) Improve this logic once ProfileInitStage and
  // AppInitStage are fully decoupled.
  if (initStage >= AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    for (ProfileState* profileState in self.connectedProfileStates) {
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
  }
  self.isIncrementingInitStage = NO;

  if (self.needsIncrementInitStage) {
    self.needsIncrementInitStage = NO;
    [self queueTransitionToNextInitStage];
  }
}

#pragma mark - PortraitOrientationManager

- (void)incrementForcePortraitOrientationCounter {
  if (!_forcePortraitOrientationCounter) {
    for (SceneState* sceneState in self.connectedScenes) {
      [sceneState.browserProviderInterface.currentBrowserProvider
              .viewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }
  }
  ++_forcePortraitOrientationCounter;
}

- (void)decrementForcePortraitOrientationCounter {
  CHECK_GT(_forcePortraitOrientationCounter, 0ul);
  --_forcePortraitOrientationCounter;
  if (!_forcePortraitOrientationCounter) {
    for (SceneState* sceneState in self.connectedScenes) {
      [sceneState.browserProviderInterface.currentBrowserProvider
              .viewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }
  }
}

#pragma mark - UIBlockerManager

- (void)incrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  DCHECK(self.uiBlockerTarget == nil || target == self.uiBlockerTarget)
      << "Another scene is already showing a blocking UI!";
  self.blockingUICounter++;
  if (!self.uiBlockerTarget) {
    self.uiBlockerTarget = target;
  }
}

- (void)decrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  DCHECK(self.blockingUICounter > 0 && self.uiBlockerTarget == target);
  self.blockingUICounter--;
  if (self.blockingUICounter == 0) {
    self.uiBlockerTarget = nil;
    [self.uiBlockerManagerObservers currentUIBlockerRemoved];
  }
}

- (id<UIBlockerTarget>)currentUIBlocker {
  return self.uiBlockerTarget;
}

- (void)addUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [self.uiBlockerManagerObservers addObserver:observer];
}

- (void)removeUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [self.uiBlockerManagerObservers removeObserver:observer];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  if (self.firstSceneHasInitializedUI) {
    return;
  }
  self.firstSceneHasInitializedUI = YES;
  [self.observers appState:self firstSceneHasInitializedUI:sceneState];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    sceneState.presentingModalOverlay =
        (self.uiBlockerTarget != nil) && (self.uiBlockerTarget != sceneState);
    [self.observers appState:self sceneDidBecomeActive:sceneState];
  }
  crash_keys::SetForegroundScenesCount([self foregroundScenes].count);
}

#pragma mark - Scenes lifecycle

- (void)sceneWillConnect:(NSNotification*)notification {
  UIWindowScene* scene =
      base::apple::ObjCCastStrict<UIWindowScene>(notification.object);
  SceneDelegate* sceneDelegate =
      base::apple::ObjCCastStrict<SceneDelegate>(scene.delegate);

  // Under some iOS 15 betas, Chrome gets scene connection events for some
  // system scene connections. To handle this, early return if the connecting
  // scene doesn't have a valid delegate. (See crbug.com/1217461)
  if (!sceneDelegate)
    return;

  SceneState* sceneState = sceneDelegate.sceneState;
  DCHECK(sceneState);

  [self.observers appState:self sceneConnected:sceneState];
  crash_keys::SetConnectedScenesCount([self connectedScenes].count);
}

#pragma mark - Voice Over lifecycle

- (void)voiceOverStatusDidChange:(NSNotification*)notification {
  crash_keys::SetVoiceOverRunning(UIAccessibilityIsVoiceOverRunning());
}

#pragma mark - AppStateObserver

// TODO(crbug.com/40756629): Move this logic to a specific agent.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (previousInitStage != AppInitStage::kBrowserObjectsForUI) {
    return;
  }

  [self completeUIInitialization];
}

#pragma mark - Private

// TODO(crbug.com/325596562): AppState should not push to ProfileState, instead
// this should be refactored. This is temporary code until each ProfileState is
// correctly managed by its ProfileController.
- (NSSet<ProfileState*>*)connectedProfileStates {
  NSMutableSet<ProfileState*>* profileStates = [[NSMutableSet alloc] init];
  for (SceneState* sceneState in self.connectedScenes) {
    ProfileState* profileState = sceneState.profileState;
    if (profileState) {
      [profileStates addObject:profileState];
    }
  }
  return profileStates;
}

@end
