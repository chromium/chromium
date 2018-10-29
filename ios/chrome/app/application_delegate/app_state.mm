// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/critical_closure.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/metrics/metrics_service.h"
#import "ios/chrome/app/application_delegate/app_navigation.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/app/startup/content_suggestions_scheduler_notifications.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/crash_loop_detection_util.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#include "ios/chrome/browser/ui/background_generator.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/browser_view_information.h"
#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/web_task_traits.h"
#include "net/url_request/url_request_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Helper method to post |closure| on the UI thread.
void PostTaskOnUIThread(base::OnceClosure closure) {
  base::PostTaskWithTraits(FROM_HERE, {web::WebThread::UI}, std::move(closure));
}
NSString* const kStartupAttemptReset = @"StartupAttempReset";
}  // namespace

@interface AppState ()<SafeModeCoordinatorDelegate> {
  // Container for startup information.
  __weak id<StartupInformation> _startupInformation;
  // Browser launcher to launch browser in different states.
  __weak id<BrowserLauncher> _browserLauncher;
  // UIApplicationDelegate for the application.
  __weak MainApplicationDelegate* _mainApplicationDelegate;
  // Window for the application.
  __weak UIWindow* _window;

  // Variables backing properties of same name.
  SafeModeCoordinator* _safeModeCoordinator;

  // Start of the current session, used for UMA.
  base::TimeTicks _sessionStartTime;
  // YES if the app is currently in the process of terminating.
  BOOL _appIsTerminating;
  // Interstitial view used to block any incognito tabs after backgrounding.
  UIView* _incognitoBlocker;
  // Whether the application is currently in the background.
  // This is a workaround for rdar://22392526 where
  // -applicationDidEnterBackground: can be called twice.
  // TODO(crbug.com/546196): Remove this once rdar://22392526 is fixed.
  BOOL _applicationInBackground;
  // YES if cookies are currently being flushed to disk.
  BOOL _savingCookies;
}

// Safe mode coordinator. If this is non-nil, the app is displaying the safe
// mode UI.
@property(nonatomic, strong) SafeModeCoordinator* safeModeCoordinator;

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
    _startupInformation = startupInformation;
    _browserLauncher = browserLauncher;
    _mainApplicationDelegate = applicationDelegate;
  }
  return self;
}

#pragma mark - Properties implementation

- (SafeModeCoordinator*)safeModeCoordinator {
  return _safeModeCoordinator;
}

- (void)setSafeModeCoordinator:(SafeModeCoordinator*)safeModeCoordinator {
  _safeModeCoordinator = safeModeCoordinator;
}

- (void)setWindow:(UIWindow*)window {
  _window = window;
}

- (UIWindow*)window {
  return _window;
}

#pragma mark - Public methods.

- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper
              incognitoContentVisible:(BOOL)incognitoContentVisible {
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

  breakpad_helper::SetCurrentlyInBackground(true);

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

  [_startupInformation expireFirstUserActionRecorder];

  // If the current BVC is incognito, or if we are in the tab switcher and there
  // are incognito tabs visible, place a full screen view containing the
  // switcher background to hide any incognito content.
  if (incognitoContentVisible) {
    // Cover the largest area potentially shown in the app switcher, in case
    // the screenshot is reused in a different orientation or size class.
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    CGFloat maxDimension =
        std::max(CGRectGetWidth(screenBounds), CGRectGetHeight(screenBounds));
    _incognitoBlocker = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, maxDimension, maxDimension)];
    NSBundle* mainBundle = base::mac::FrameworkBundle();
    NSArray* topObjects =
        [mainBundle loadNibNamed:@"LaunchScreen" owner:self options:nil];
    UIViewController* launchScreenController =
        base::mac::ObjCCastStrict<UIViewController>([topObjects lastObject]);
    [_incognitoBlocker addSubview:[launchScreenController view]];
    [launchScreenController view].autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    _incognitoBlocker.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    [_window addSubview:_incognitoBlocker];
  }

  // Do not save cookies if it is already in progress.
  if ([[_browserLauncher browserViewInformation] currentBVC].browserState &&
      !_savingCookies) {
    // Save cookies to disk. The empty critical closure guarantees that the task
    // will be run before backgrounding.
    scoped_refptr<net::URLRequestContextGetter> getter =
        [[_browserLauncher browserViewInformation] currentBVC]
            .browserState->GetRequestContext();
    _savingCookies = YES;
    __block base::OnceClosure criticalClosure =
        base::MakeCriticalClosure(base::BindOnce(^{
          DCHECK_CURRENTLY_ON(web::WebThread::UI);
          _savingCookies = NO;
        }));
    base::PostTaskWithTraits(
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

  // Turn off uploading of crash reports and metrics, in case the method of
  // communication changes while in the background.
  [MetricsMediator disableReporting];

  GetApplicationContext()->OnAppEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper
                             tabOpener:(id<TabOpening>)tabOpener
                         appNavigation:(id<AppNavigation>)appNavigation {
  if ([_browserLauncher browserInitializationStage] <
      INITIALIZATION_STAGE_FOREGROUND) {
    // The application has been launched in background and the initialization
    // is not complete.
    [self initializeUI];
    return;
  }
  if ([self isInSafeMode])
    return;

  _applicationInBackground = NO;

  [_incognitoBlocker removeFromSuperview];
  _incognitoBlocker = nil;

  breakpad_helper::SetCurrentlyInBackground(false);

  // Update the state of metrics and crash reporting, as the method of
  // communication may have changed while the app was in the background.
  [metricsMediator updateMetricsStateBasedOnPrefsUserTriggered:NO];

  // Send any feedback that might be still on temporary storage.
  ios::GetChromeBrowserProvider()->GetUserFeedbackProvider()->Synchronize();

  GetApplicationContext()->OnAppEnterForeground();

  [MetricsMediator
      logLaunchMetricsWithStartupInformation:_startupInformation
                      browserViewInformation:[_browserLauncher
                                                 browserViewInformation]];
  [memoryHelper resetForegroundMemoryWarningCount];

  ios::ChromeBrowserState* currentBrowserState =
      [[_browserLauncher browserViewInformation] currentBrowserState];
  if ([SignedInAccountsViewController
          shouldBePresentedForBrowserState:currentBrowserState]) {
    [appNavigation presentSignedInAccountsViewControllerForBrowserState:
                       currentBrowserState];
  }

  // Use the mainBVC as the ContentSuggestions can only be started in non-OTR.
  [ContentSuggestionsSchedulerNotifications
      notifyForeground:[[[_browserLauncher browserViewInformation] mainBVC]
                           browserState]];

  // If the current browser state is not OTR, check for cookie loss.
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
}

- (void)resumeSessionWithTabOpener:(id<TabOpening>)tabOpener
                       tabSwitcher:(id<TabSwitching>)tabSwitcher {
  [_incognitoBlocker removeFromSuperview];
  _incognitoBlocker = nil;

  DCHECK([_browserLauncher browserInitializationStage] ==
         INITIALIZATION_STAGE_FOREGROUND);
  _sessionStartTime = base::TimeTicks::Now();
  [[[_browserLauncher browserViewInformation] mainTabModel]
      resetSessionMetrics];

  if ([_startupInformation startupParameters]) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                          startupInformation:_startupInformation
                      browserViewInformation:[_browserLauncher
                                                 browserViewInformation]];
  } else if ([tabOpener shouldOpenNTPTabOnActivationOfTabModel:
                            [[_browserLauncher browserViewInformation]
                                currentTabModel]]) {
    // Opens an NTP if needed.
    // TODO(crbug.com/623491): opening a tab when the application is launched
    // without a tab should not be counted as a user action. Revisit the way tab
    // creation is counted.
    if (![tabSwitcher openNewTabFromTabSwitcher]) {
      BrowserViewController* bvc =
          [[_browserLauncher browserViewInformation] currentBVC];
      BOOL incognito =
          bvc == [[_browserLauncher browserViewInformation] otrBVC];
      [bvc.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithIncognito:incognito]];
    }
  } else {
    [[[_browserLauncher browserViewInformation] currentBVC]
        presentBubblesIfEligible];
  }

  [MetricsMediator logStartupDuration:_startupInformation];
}

- (void)applicationWillTerminate:(UIApplication*)application
           applicationNavigation:(id<AppNavigation>)appNavigation {
  if (_appIsTerminating) {
    // Previous handling of this method spun the runloop, resulting in
    // recursive calls; this does not appear to happen with the new shutdown
    // flow, but this is here to ensure that if it can happen, it gets noticed
    // and fixed.
    CHECK(false);
  }
  _appIsTerminating = YES;

  // Dismiss any UI that is presented on screen and that is listening for
  // profile notifications.
  if ([appNavigation settingsNavigationController])
    [appNavigation closeSettingsAnimated:NO completion:nil];

  // Clean up the device sharing manager before the main browser state is shut
  // down.
  if ([_browserLauncher browserInitializationStage] >=
      INITIALIZATION_STAGE_FOREGROUND) {
    [[_browserLauncher browserViewInformation] cleanDeviceSharingManager];
  }

  // Cancel any in-flight distribution notifications.
  ios::GetChromeBrowserProvider()
      ->GetAppDistributionProvider()
      ->CancelDistributionNotifications();

  // Halt the tabs, so any outstanding requests get cleaned up, without actually
  // closing the tabs. Set the BVC to inactive to cancel all the dialogs.
  if ([_browserLauncher browserInitializationStage] >=
      INITIALIZATION_STAGE_FOREGROUND) {
    [[_browserLauncher browserViewInformation] haltAllTabs];
    [_browserLauncher browserViewInformation].currentBVC.active = NO;
  }

  // TODO(crbug.com/585700): remove this.
  web::RequestTrackerImpl::BlockUntilTrackersShutdown();

  [_startupInformation stopChromeMain];
}

- (void)willResignActiveTabModel {
  if ([_browserLauncher browserInitializationStage] <
      INITIALIZATION_STAGE_FOREGROUND) {
    // If the application did not pass the foreground initialization stage,
    // there is no active tab model to resign.
    return;
  }

  // Set [_startupInformation isColdStart] to NO in anticipation of the next
  // time the app becomes active.
  [_startupInformation setIsColdStart:NO];

  base::TimeDelta duration = base::TimeTicks::Now() - _sessionStartTime;
  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", duration);
  [[[_browserLauncher browserViewInformation] mainTabModel]
      recordSessionMetrics];
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

- (BOOL)isInSafeMode {
  return self.safeModeCoordinator != nil;
}

- (void)launchFromURLHandled:(BOOL)URLHandled {
  self.shouldPerformAdditionalDelegateHandling = !URLHandled;
}

#pragma mark - SafeModeCoordinatorDelegate Implementation

- (void)coordinatorDidExitSafeMode:(nonnull SafeModeCoordinator*)coordinator {
  self.safeModeCoordinator = nil;
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];
  [_mainApplicationDelegate
      applicationDidBecomeActive:[UIApplication sharedApplication]];
}

#pragma mark - Internal methods.

- (void)initializeUI {
  _userInteracted = YES;
  [self saveLaunchDetailsToDefaults];

  DCHECK([_window rootViewController] == nil);
  if ([SafeModeCoordinator shouldStart]) {
    SafeModeCoordinator* safeModeCoordinator =
        [[SafeModeCoordinator alloc] initWithWindow:_window];

    self.safeModeCoordinator = safeModeCoordinator;
    [self.safeModeCoordinator setDelegate:self];

    // Activate the main window, which will prompt the views to load.
    [_window makeKeyAndVisible];

    [self.safeModeCoordinator start];
    return;
  }

  // Don't add code here. Add it in MainController's
  // -startUpBrowserForegroundInitialization.
  DCHECK([_startupInformation isColdStart]);
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];
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

@end
