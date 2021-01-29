// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/critical_closure.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/main_application_delegate.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/sessions_storage_util.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/features.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/help_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_delegate.h"
#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/web_state_list_metrics_browser_agent.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Helper method to post |closure| on the UI thread.
void PostTaskOnUIThread(base::OnceClosure closure) {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, std::move(closure));
}
NSString* const kStartupAttemptReset = @"StartupAttempReset";

// Time interval used for startRecordingMemoryFootprintWithInterval:
const NSTimeInterval kMemoryFootprintRecordingTimeInterval = 5;

}  // namespace

#pragma mark - AppStateObserverList

@interface AppStateObserverList : CRBProtocolObservers <AppStateObserver>
@end

@implementation AppStateObserverList
@end

#pragma mark - AppState

@interface AppState () <SafeModeCoordinatorDelegate> {
  // Browser launcher to launch browser in different states.
  __weak id<BrowserLauncher> _browserLauncher;
  // UIApplicationDelegate for the application.
  __weak MainApplicationDelegate* _mainApplicationDelegate;

  // Variables backing properties of same name.
  SafeModeCoordinator* _safeModeCoordinator;

  // YES if the app is currently in the process of terminating.
  BOOL _appIsTerminating;
  // Whether the application is currently in the background.
  // This is a workaround for rdar://22392526 where
  // -applicationDidEnterBackground: can be called twice.
  // TODO(crbug.com/546196): Remove this once rdar://22392526 is fixed.
  BOOL _applicationInBackground;
  // YES if cookies are currently being flushed to disk.
  BOOL _savingCookies;

  // Multiwindow UI blocker used when safe mode is active.
  std::unique_ptr<ScopedUIBlocker> _safeModeBlocker;
}

// Container for observers.
@property(nonatomic, strong) AppStateObserverList* observers;

// Safe mode coordinator. If this is non-nil, the app is displaying the safe
// mode UI.
@property(nonatomic, strong) SafeModeCoordinator* safeModeCoordinator;

// Flag to track when the app is in safe mode.
@property(nonatomic, assign, getter=isInSafeMode) BOOL inSafeMode;

// Return value for -requiresHandlingAfterLaunchWithOptions that determines if
// UIKit should make followup delegate calls such as
// -performActionForShortcutItem or -openURL.
@property(nonatomic, assign) BOOL shouldPerformAdditionalDelegateHandling;

// This method is the first to be called when user launches the application.
// Depending on the background tasks history, the state of the application is
// either INITIALIZATION_STAGE_BASIC or INITIALIZATION_STAGE_BACKGROUND so this
// step cannot be included in the |startUpBrowserToStage:| method.
- (void)initializeUI;
// Saves the current launch details to user defaults.
- (void)saveLaunchDetailsToDefaults;

// This flag is set when the first scene has activated since the startup, and
// never reset.
@property(nonatomic, assign) BOOL firstSceneHasActivated;

// This flag is set when the first scene has initialized its UI and never reset.
@property(nonatomic, assign) BOOL firstSceneHasInitializedUI;

// The current blocker target if any.
@property(nonatomic, weak, readwrite) id<UIBlockerTarget> uiBlockerTarget;

// The counter of currently shown blocking UIs. Do not use this directly,
// instead use incrementBlockingUICounterForScene: and
// incrementBlockingUICounterForScene or the ScopedUIBlocker.
@property(nonatomic, assign) NSUInteger blockingUICounter;

// Agents attached to this app state.
@property(nonatomic, strong) NSMutableArray<id<AppStateAgent>>* agents;

@end

@implementation AppState

@synthesize shouldPerformAdditionalDelegateHandling =
    _shouldPerformAdditionalDelegateHandling;
@synthesize userInteracted = _userInteracted;

- (instancetype)
initWithBrowserLauncher:(id<BrowserLauncher>)browserLauncher
     startupInformation:(id<StartupInformation>)startupInformation
    applicationDelegate:(MainApplicationDelegate*)applicationDelegate {
  self = [super init];
  if (self) {
    _observers = [AppStateObserverList
        observersWithProtocol:@protocol(AppStateObserver)];
    _agents = [[NSMutableArray alloc] init];
    _startupInformation = startupInformation;
    _browserLauncher = browserLauncher;
    _mainApplicationDelegate = applicationDelegate;
    _appCommandDispatcher = [[CommandDispatcher alloc] init];

    // Subscribe to scene-related notifications when using scenes.
    // Note these are also sent when not using scenes, so avoid subscribing to
    // them unless necessary.
    if (base::ios::IsSceneStartupSupported()) {
      if (@available(iOS 13, *)) {
        // Subscribe to scene connection notifications.
        [[NSNotificationCenter defaultCenter]
            addObserver:self
               selector:@selector(sceneWillConnect:)
                   name:UISceneWillConnectNotification
                 object:nil];
      }
    }
  }
  return self;
}

#pragma mark - Properties implementation

- (void)setMainSceneState:(SceneState*)mainSceneState {
  DCHECK(!_mainSceneState);
  _mainSceneState = mainSceneState;
  [self.observers appState:self sceneConnected:mainSceneState];
}

- (SafeModeCoordinator*)safeModeCoordinator {
  return _safeModeCoordinator;
}

- (void)setSafeModeCoordinator:(SafeModeCoordinator*)safeModeCoordinator {
  _safeModeCoordinator = safeModeCoordinator;
}

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

#pragma mark - Public methods.

- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper {
  if ([self isInSafeMode]) {
    // Force a crash when backgrounding and in safe mode, so users don't get
    // stuck in safe mode.
    breakpad_helper::SetEnabled(false);
    exit(0);
    return;
  }

  if (_applicationInBackground) {
    return;
  }
  _applicationInBackground = YES;

  ChromeBrowserState* browserState =
      _browserLauncher.interfaceProvider.mainInterface.browserState;
  if (browserState) {
    AuthenticationServiceFactory::GetForBrowserState(browserState)
        ->OnApplicationDidEnterBackground();
  }

  crash_keys::SetCurrentlyInBackground(true);

  if ([_browserLauncher browserInitializationStage] <
      INITIALIZATION_STAGE_FOREGROUND) {
    // The clean-up done in |-applicationDidEnterBackground:| is only valid for
    // the case when the application is started in foreground, so there is
    // nothing to clean up as the application was not initialized for foregound.
    //
    // From the stack trace of the crash bug http://crbug.com/437307 , it
    // seems that |-applicationDidEnterBackground:| may be called when the app
    // is started in background and before the initialization for background
    // stage is done. Note that the crash bug could not be reproduced though.
    return;
  }

  [MetricsMediator
      applicationDidEnterBackground:[memoryHelper
                                        foregroundMemoryWarningCount]];

  [self.startupInformation expireFirstUserActionRecorder];

  // Do not save cookies if it is already in progress.
  id<BrowserInterface> currentInterface =
      _browserLauncher.interfaceProvider.currentInterface;
  if (currentInterface.browserState && !_savingCookies) {
    // Save cookies to disk. The empty critical closure guarantees that the task
    // will be run before backgrounding.
    scoped_refptr<net::URLRequestContextGetter> getter =
        currentInterface.browserState->GetRequestContext();
    _savingCookies = YES;
    __block base::OnceClosure criticalClosure = base::MakeCriticalClosure(
        "applicationDidEnterBackground:_savingCookies", base::BindOnce(^{
          DCHECK_CURRENTLY_ON(web::WebThread::UI);
          _savingCookies = NO;
        }));
    base::PostTask(
        FROM_HERE, {web::WebThread::IO}, base::BindOnce(^{
          net::CookieStoreIOS* store = static_cast<net::CookieStoreIOS*>(
              getter->GetURLRequestContext()->cookie_store());
          // FlushStore() runs its callback on any thread. Jump back to UI.
          store->FlushStore(
              base::BindOnce(&PostTaskOnUIThread, std::move(criticalClosure)));
        }));
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

  // Turn off uploading of crash reports and metrics, in case the method of
  // communication changes while in the background.
  [MetricsMediator disableReporting];

  GetApplicationContext()->OnAppEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper {
  if ([_browserLauncher browserInitializationStage] <
      INITIALIZATION_STAGE_FOREGROUND) {
    // The application has been launched in background and the initialization
    // is not complete.
    [self initializeUI];
    return;
  }
  if ([self isInSafeMode] || !_applicationInBackground)
    return;

  _applicationInBackground = NO;
  ChromeBrowserState* browserState =
      _browserLauncher.interfaceProvider.mainInterface.browserState;
  if (browserState) {
    AuthenticationServiceFactory::GetForBrowserState(browserState)
        ->OnApplicationWillEnterForeground();
  }

  crash_keys::SetCurrentlyInBackground(false);

  // Update the state of metrics and crash reporting, as the method of
  // communication may have changed while the app was in the background.
  [metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Send any feedback that might be still on temporary storage.
  ios::GetChromeBrowserProvider()->GetUserFeedbackProvider()->Synchronize();

  GetApplicationContext()->OnAppEnterForeground();

  [MetricsMediator
      logLaunchMetricsWithStartupInformation:self.startupInformation
                             connectedScenes:self.connectedScenes];
  [memoryHelper resetForegroundMemoryWarningCount];

  // If the current browser state is not OTR, check for cookie loss.
  ChromeBrowserState* currentBrowserState =
      _browserLauncher.interfaceProvider.currentInterface.browserState;
  if (currentBrowserState && !currentBrowserState->IsOffTheRecord() &&
      currentBrowserState->GetOriginalChromeBrowserState()
              ->GetStatePath()
              .BaseName()
              .value() == kIOSChromeInitialBrowserState) {
    NSUInteger cookie_count =
        [[[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies] count];
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieCountOnForegrounding",
                               cookie_count);
    net::CheckForCookieLoss(cookie_count,
                            net::COOKIES_APPLICATION_FOREGROUNDED);
  }

  if (currentBrowserState) {
    // Send the "Chrome Opened" event to the feature_engagement::Tracker on a
    // warm start.
    feature_engagement::TrackerFactory::GetForBrowserState(currentBrowserState)
        ->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  base::RecordAction(base::UserMetricsAction("MobileWillEnterForeground"));

  if (EnableSyntheticCrashReportsForUte()) {
    [[PreviousSessionInfo sharedInstance]
        startRecordingMemoryFootprintWithInterval:
            base::TimeDelta::FromSeconds(
                kMemoryFootprintRecordingTimeInterval)];
  }
}

- (void)resumeSessionWithTabOpener:(id<TabOpening>)tabOpener
                       tabSwitcher:(id<TabSwitching>)tabSwitcher
             connectionInformation:
                 (id<ConnectionInformation>)connectionInformation {
  DCHECK(!base::ios::IsSceneStartupSupported());
  DCHECK([_browserLauncher browserInitializationStage] ==
         INITIALIZATION_STAGE_FOREGROUND);

  id<BrowserInterface> currentInterface =
      _browserLauncher.interfaceProvider.currentInterface;
  CommandDispatcher* dispatcher =
      currentInterface.browser->GetCommandDispatcher();
  if ([connectionInformation startupParameters]) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                       connectionInformation:connectionInformation
                          startupInformation:self.startupInformation
                                browserState:currentInterface.browserState];
  } else if ([tabOpener shouldOpenNTPTabOnActivationOfBrowser:currentInterface
                                                                  .browser]) {
    // Opens an NTP if needed.
    // TODO(crbug.com/623491): opening a tab when the application is launched
    // without a tab should not be counted as a user action. Revisit the way tab
    // creation is counted.
    if (![tabSwitcher openNewTabFromTabSwitcher]) {
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithIncognito:currentInterface.incognito];
      [HandlerForProtocol(dispatcher, ApplicationCommands)
          openURLInNewTab:command];
    }
  } else {
    [HandlerForProtocol(dispatcher, HelpCommands) showHelpBubbleIfEligible];
  }

  [MetricsMediator logStartupDuration:self.startupInformation
                connectionInformation:connectionInformation];
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
  CHECK(ios::GetChromeBrowserProvider());
  ios::GetChromeBrowserProvider()
      ->GetAppDistributionProvider()
      ->CancelDistributionNotifications();

  // Halt the tabs, so any outstanding requests get cleaned up, without actually
  // closing the tabs. Set the BVC to inactive to cancel all the dialogs.
  // Don't do this if there are no scenes, since there's no defined interface
  // provider (and no tabs)
  // TODO(crbug.com/1113097): Factor out this check by not having app layer
  // logic use interface providers.
  BOOL scenesAreAvailable = [self connectedScenes].count > 0;

  if (scenesAreAvailable && [_browserLauncher browserInitializationStage] >=
                                INITIALIZATION_STAGE_FOREGROUND) {
    _browserLauncher.interfaceProvider.currentInterface.userInteractionEnabled =
        NO;
  }

  // Trigger UI teardown on iOS 12.
  if (!base::ios::IsSceneStartupSupported()) {
    self.mainSceneState.activationLevel = SceneActivationLevelUnattached;
  }

  [self.startupInformation stopChromeMain];
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions
    API_AVAILABLE(ios(13)) {
  NSMutableArray<NSString*>* sessionIDs =
      [NSMutableArray arrayWithCapacity:sceneSessions.count];
  // This method is invoked by iOS to inform the application that the sessions
  // for "closed windows" is garbage collected and that any data associated with
  // them by the application needs to be deleted.
  //
  // Usually Chrome uses -[SceneState sceneSessionID] as identifier to properly
  // support devices that do not support multi-window (and which use a constant
  // identifier). For devices that do not support multi-window the session is
  // saved at a constant path, so it is harmnless to delete files at a path
  // derived from -persistentIdentifier (since there won't be files deleted).
  // For devices that do support multi-window, there is data to delete once the
  // session is garbage collected.
  //
  // Thus it is always correct to use -persistentIdentifier here.
  for (UISceneSession* session in sceneSessions) {
    [sessionIDs addObject:session.persistentIdentifier];
  }
  sessions_storage_util::MarkSessionsForRemoval(sessionIDs);
}

- (void)willResignActiveTabModel {
  if ([_browserLauncher browserInitializationStage] <
      INITIALIZATION_STAGE_FOREGROUND) {
    // If the application did not pass the foreground initialization stage,
    // there is no active tab model to resign.
    return;
  }

  // Set [self.startupInformation isColdStart] to NO in anticipation of the next
  // time the app becomes active.
  [self.startupInformation setIsColdStart:NO];

  id<BrowserInterface> currentInterface =
      _browserLauncher.interfaceProvider.currentInterface;

  // Record session metrics (currentInterface.browserState may be null during
  // tests).
  if (currentInterface.browserState) {
    ChromeBrowserState* mainChromeBrowserState =
        currentInterface.browserState->GetOriginalChromeBrowserState();

    SessionMetrics::FromBrowserState(mainChromeBrowserState)
        ->RecordAndClearSessionMetrics(
            MetricsToRecordFlags::kOpenedTabCount |
            MetricsToRecordFlags::kClosedTabCount |
            MetricsToRecordFlags::kActivatedTabCount);

    if (mainChromeBrowserState->HasOffTheRecordChromeBrowserState()) {
      ChromeBrowserState* otrChromeBrowserState =
          mainChromeBrowserState->GetOffTheRecordChromeBrowserState();

      SessionMetrics::FromBrowserState(otrChromeBrowserState)
          ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);
    }
  }
}

- (BOOL)requiresHandlingAfterLaunchWithOptions:(NSDictionary*)launchOptions
                               stateBackground:(BOOL)stateBackground {
  [_browserLauncher setLaunchOptions:launchOptions];
  self.shouldPerformAdditionalDelegateHandling = YES;

  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_BASIC];
  if (!stateBackground) {
    [self initializeUI];
  }

  return self.shouldPerformAdditionalDelegateHandling;
}

- (void)launchFromURLHandled:(BOOL)URLHandled {
  self.shouldPerformAdditionalDelegateHandling = !URLHandled;
}

- (void)addObserver:(id<SceneStateObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SceneStateObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)addAgent:(id<AppStateAgent>)agent {
  DCHECK(agent);
  [self.agents addObject:agent];
  [agent setAppState:self];
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
  if (base::ios::IsSceneStartupSupported()) {
    if (@available(iOS 13, *)) {
      NSMutableArray* sceneStates = [[NSMutableArray alloc] init];
      NSSet* connectedScenes =
          [UIApplication sharedApplication].connectedScenes;
      for (UIWindowScene* scene in connectedScenes) {
        if (![scene.delegate isKindOfClass:[SceneDelegate class]]) {
          // This might happen in tests.
          // TODO(crbug.com/1113097): This shouldn't be needed. (It might also
          // be the cause of crbug.com/1142782).
          [sceneStates addObject:[[SceneState alloc] initWithAppState:self]];
          continue;
        }

        SceneDelegate* sceneDelegate =
            base::mac::ObjCCastStrict<SceneDelegate>(scene.delegate);
        [sceneStates addObject:sceneDelegate.sceneState];
      }
      return sceneStates;
    }
  } else if (self.mainSceneState) {
    return @[ self.mainSceneState ];
  }
  // This can happen if the app is terminating before any scenes are set up.
  return @[];
}

- (void)setLastTappedWindow:(UIWindow*)window {
  if (_lastTappedWindow == window) {
    return;
  }
  _lastTappedWindow = window;
  [self.observers appState:self lastTappedWindowChanged:window];
}

#pragma mark - SafeModeCoordinatorDelegate Implementation

- (void)coordinatorDidExitSafeMode:(nonnull SafeModeCoordinator*)coordinator {
  [self stopSafeMode];
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];
  [self.observers appStateDidExitSafeMode:self];

  [_mainApplicationDelegate
      applicationDidBecomeActive:[UIApplication sharedApplication]];
}

#pragma mark - Internal methods.

- (void)startSafeMode {
  if (!base::ios::IsSceneStartupSupported()) {
    self.mainSceneState.activationLevel = SceneActivationLevelForegroundActive;
  }
  DCHECK(self.foregroundActiveScene);
  DCHECK(!_safeModeBlocker);
  SafeModeCoordinator* safeModeCoordinator = [[SafeModeCoordinator alloc]
      initWithWindow:self.foregroundActiveScene.window];

  self.safeModeCoordinator = safeModeCoordinator;
  [self.safeModeCoordinator setDelegate:self];

  // Activate the main window, which will prompt the views to load.
  [self.foregroundActiveScene.window makeKeyAndVisible];

  [self.safeModeCoordinator start];

  if (base::ios::IsMultipleScenesSupported()) {
    _safeModeBlocker =
        std::make_unique<ScopedUIBlocker>(self.foregroundActiveScene);
  }
}

- (void)stopSafeMode {
  if (_safeModeBlocker) {
    _safeModeBlocker.reset();
  }
  self.safeModeCoordinator = nil;
  self.inSafeMode = NO;
}

- (void)initializeUI {
  _userInteracted = YES;
  [self saveLaunchDetailsToDefaults];

  if ([SafeModeCoordinator shouldStart]) {
    self.inSafeMode = YES;
    if (!base::ios::IsMultiwindowSupported()) {
      // Start safe mode immediately. Otherwise it should only start when a
      // scene is connected and activates to allow displaying the safe mode UI.
      [self startSafeMode];
    }
    return;
  }

  // Don't add code here. Add it in MainController's
  // -startUpBrowserForegroundInitialization.
  DCHECK([self.startupInformation isColdStart]);
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  if (EnableSyntheticCrashReportsForUte()) {
    // Must be called after sequenced context creation, which happens in
    // startUpBrowserToStage: method called above.
    [[PreviousSessionInfo sharedInstance]
        startRecordingMemoryFootprintWithInterval:
            base::TimeDelta::FromSeconds(
                kMemoryFootprintRecordingTimeInterval)];
  }
}

- (void)saveLaunchDetailsToDefaults {
  // Reset the failure count on first launch, increment it on other launches.
  if ([[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade])
    crash_util::ResetFailedStartupAttemptCount();
  else
    crash_util::IncrementFailedStartupAttemptCount(false);

  // The startup failure count *must* be synchronized now, since the crashes it
  // is trying to count are during startup.
  // -[PreviousSessionInfo beginRecordingCurrentSession] calls |synchronize| on
  // the user defaults, so leverage that to prevent calling it twice.

  // Start recording info about this session.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
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
  }
}

- (id<UIBlockerTarget>)currentUIBlocker {
  return self.uiBlockerTarget;
}

#pragma mark - SceneStateObserver

- (void)sceneStateHasInitializedUI:(SceneState*)sceneState {
  if (self.firstSceneHasInitializedUI) {
    return;
  }
  self.firstSceneHasInitializedUI = YES;
  [self.observers appState:self firstSceneHasInitializedUI:sceneState];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    if (!self.firstSceneHasActivated) {
      self.firstSceneHasActivated = YES;
      if (self.isInSafeMode) {
        // Safe mode can only be started when there's a window, so the actual
        // safe mode has been postponed until now.
        [self startSafeMode];
      }
    }
    sceneState.presentingModalOverlay =
        (self.uiBlockerTarget != nil) && (self.uiBlockerTarget != sceneState);
  }
}

- (void)sceneWillConnect:(NSNotification*)notification {
  DCHECK(base::ios::IsSceneStartupSupported());
  if (@available(iOS 13, *)) {
    UIWindowScene* scene =
        base::mac::ObjCCastStrict<UIWindowScene>(notification.object);
    SceneDelegate* sceneDelegate =
        base::mac::ObjCCastStrict<SceneDelegate>(scene.delegate);
    SceneState* sceneState = sceneDelegate.sceneState;
    DCHECK(sceneState);

    [self.observers appState:self sceneConnected:sceneState];
  }
}

@end
