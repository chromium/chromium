// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_application_delegate.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_delegate.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/web/common/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The time delay after firstSceneWillEnterForeground: before checking for main
// intent signals.
const int kMainIntentCheckDelay = 1;
}  // namespace

@interface MainApplicationDelegate () <AppStateObserver> {
  MainController* _mainController;
  // Memory helper used to log the number of memory warnings received.
  MemoryWarningHelper* _memoryHelper;
  // Metrics mediator used to check and update the metrics accordingly to
  // to the user preferences.
  MetricsMediator* _metricsMediator;
  // Browser launcher to have a global launcher.
  id<BrowserLauncher> _browserLauncher;
  // Container for startup information.
  id<StartupInformation> _startupInformation;
  // Helper to open new tabs.
  id<TabOpening> _tabOpener;
  // Handles tab switcher.
  id<TabSwitching> _tabSwitcher;
  // The set of "scene sessions" that needs to be discarded. See
  // -applicatiopn:didDiscardSceneSessions: for details.
  NSSet<UISceneSession*>* _sceneSessionsToDiscard;
}

// YES if application:didFinishLaunchingWithOptions: was called. Used to
// determine whether or not shutdown should be invoked from
// applicationWillTerminate:.
@property(nonatomic, assign) BOOL didFinishLaunching;

@end

@implementation MainApplicationDelegate

- (instancetype)init {
  if (self = [super init]) {
    _memoryHelper = [[MemoryWarningHelper alloc] init];
    _mainController = [[MainController alloc] init];
    _metricsMediator = [[MetricsMediator alloc] init];
    [_mainController setMetricsMediator:_metricsMediator];
    _browserLauncher = _mainController;
    _startupInformation = _mainController;
    _appState = [[AppState alloc] initWithBrowserLauncher:_browserLauncher
                                       startupInformation:_startupInformation
                                      applicationDelegate:self];
    [_mainController setAppState:_appState];
  }
  return self;
}

#pragma mark - UIApplicationDelegate methods -

#pragma mark Responding to App State Changes and System Events
// Called by the OS to create the UI for display.  The UI will not be displayed,
// even if it is ready, until this function returns.
// The absolute minimum work should be done here, to ensure that the application
// startup is fast, and the UI appears as soon as possible.
- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.didFinishLaunching = YES;

  _appState.startupInformation.didFinishLaunchingTime = base::TimeTicks::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults
      setInteger:[defaults integerForKey:
                               metrics_mediator::
                                   kAppDidFinishLaunchingConsecutiveCallsKey] +
                 1
          forKey:metrics_mediator::kAppDidFinishLaunchingConsecutiveCallsKey];
  BOOL inBackground =
      [application applicationState] == UIApplicationStateBackground;
  // `inBackground` is wrongly always YES, even in regular foreground launches.
  // TODO(crbug.com/1346512): Remove this code path after some time in
  // canary. This is meant to be easy to revert.
  DCHECK(inBackground);
  BOOL requiresHandling =
      [_appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                        stateBackground:inBackground];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(sceneWillConnect:)
             name:UISceneWillConnectNotification
           object:nil];
  // UIApplicationWillResignActiveNotification is delivered before the last
  // scene has entered the background.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lastSceneWillEnterBackground:)
             name:UIApplicationWillResignActiveNotification
           object:nil];
  // UIApplicationDidEnterBackgroundNotification is delivered after the last
  // scene has entered the background.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lastSceneDidEnterBackground:)
             name:UIApplicationDidEnterBackgroundNotification
           object:nil];
  // UIApplicationWillEnterForegroundNotification will be delivered right
  // after the first scene sends UISceneWillEnterForegroundNotification.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(firstSceneWillEnterForeground:)
             name:UIApplicationWillEnterForegroundNotification
           object:nil];

  return requiresHandling;
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // If `self.didFinishLaunching` is NO, that indicates that the app was
  // terminated before startup could be run. In this situation, skip running
  // shutdown, since the app was never fully started.
  if (!self.didFinishLaunching)
    return;

  if (_appState.initStage <= InitStageSafeMode)
    return;

  // Instead of adding code here, consider if it could be handled by listening
  // for  UIApplicationWillterminate.
  [_appState applicationWillTerminate:application];
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
  if (_appState.initStage <= InitStageSafeMode)
    return;

  [_memoryHelper handleMemoryPressure];
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
  // This method is invoked by iOS to inform the application that the sessions
  // for "closed windows" is garbage collected and that any data associated with
  // them by the application needs to be deleted.
  //
  // The documentation says that if the application is not running when the OS
  // decides to discard the sessions, then it will call this method the next
  // time the application starts up. As seen by crbug.com/1292641, this call
  // happens before -[UIApplicationDelegate sceneWillConnect:] which means
  // that it can happen before Chrome has properly initialized. In that case,
  // record the list of sessions to discard and clean them once Chrome is
  // initialized.
  if (_appState.initStage <= InitStageBrowserObjectsForBackgroundHandlers) {
    _sceneSessionsToDiscard = [sceneSessions copy];
    [_appState addObserver:self];
    return;
  }

  [_appState application:application didDiscardSceneSessions:sceneSessions];
}

- (UIInterfaceOrientationMask)application:(UIApplication*)application
    supportedInterfaceOrientationsForWindow:(UIWindow*)window {
  if (_appState.portraitOnly) {
    return UIInterfaceOrientationMaskPortrait;
  }
  // Apply a no-op mask by default.
  return UIInterfaceOrientationMaskAll;
}

#pragma mark - Scenes lifecycle

- (NSInteger)foregroundSceneCount {
  NSInteger foregroundSceneCount = 0;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if ((scene.activationState == UISceneActivationStateForegroundInactive) ||
        (scene.activationState == UISceneActivationStateForegroundActive)) {
      foregroundSceneCount++;
    }
  }
  return foregroundSceneCount;
}

- (void)sceneWillConnect:(NSNotification*)notification {
  UIWindowScene* scene =
      base::mac::ObjCCastStrict<UIWindowScene>(notification.object);
  SceneDelegate* sceneDelegate =
      base::mac::ObjCCastStrict<SceneDelegate>(scene.delegate);

  // Under some iOS 15 betas, Chrome gets scene connection events for some
  // system scene connections. To handle this, early return if the connecting
  // scene doesn't have a valid delegate. (See crbug.com/1217461)
  if (!sceneDelegate)
    return;

  SceneController* sceneController = sceneDelegate.sceneController;
  _tabSwitcher = sceneController;
  _tabOpener = sceneController;

  // TODO(crbug.com/1060645): This should be called later, or this flow should
  // be changed completely.
  if (self.foregroundSceneCount == 0) {
    [_appState applicationWillEnterForeground:UIApplication.sharedApplication
                              metricsMediator:_metricsMediator
                                 memoryHelper:_memoryHelper];
  }
}

- (void)lastSceneWillEnterBackground:(NSNotification*)notification {
  if (_appState.initStage <= InitStageSafeMode)
    return;

  [_appState willResignActive];
}

- (void)lastSceneDidEnterBackground:(NSNotification*)notification {
  // Reset `startupHadExternalIntent` for all Scenes in case external intents
  // were triggered while the application was in the foreground.
  for (SceneState* scene in self.appState.connectedScenes) {
    if (scene.startupHadExternalIntent) {
      scene.startupHadExternalIntent = NO;
    }
  }
  [_appState applicationDidEnterBackground:UIApplication.sharedApplication
                              memoryHelper:_memoryHelper];
}

- (void)firstSceneWillEnterForeground:(NSNotification*)notification {
  __weak MainApplicationDelegate* weakSelf = self;
  // Delay Main Intent check since signals for intents like spotlight actions
  // are not guaranteed to occur before firstSceneWillEnterForeground.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    static_cast<int64_t>(kMainIntentCheckDelay * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        MainApplicationDelegate* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        BOOL appStartupFromExternalIntent = NO;
        for (SceneState* scene in strongSelf.appState.connectedScenes) {
          if (scene.startupHadExternalIntent) {
            appStartupFromExternalIntent = YES;
            scene.startupHadExternalIntent = NO;
          }
        }
        if (!appStartupFromExternalIntent) {
          base::RecordAction(base::UserMetricsAction("IOSOpenByMainIntent"));
        } else {
          base::RecordAction(base::UserMetricsAction("IOSOpenByViewIntent"));
        }
      });
  [_appState applicationWillEnterForeground:UIApplication.sharedApplication
                            metricsMediator:_metricsMediator
                               memoryHelper:_memoryHelper];
}

#pragma mark - AppStateObserver methods

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  DCHECK_EQ(_appState, appState);

  // The app transitioned to InitStageBrowserObjectsForBackgroundHandlers
  // or past that stage.
  if (_appState.initStage >= InitStageBrowserObjectsForBackgroundHandlers) {
    DCHECK(_sceneSessionsToDiscard);
    [_appState removeObserver:self];
    [_appState application:[UIApplication sharedApplication]
        didDiscardSceneSessions:_sceneSessionsToDiscard];
    _sceneSessionsToDiscard = nil;
  }
}

#pragma mark - Testing methods

- (MainController*)mainController {
  return _mainController;
}

- (AppState*)appState {
  return _appState;
}

+ (AppState*)sharedAppState {
  return base::mac::ObjCCast<MainApplicationDelegate>(
             [[UIApplication sharedApplication] delegate])
      .appState;
}

+ (MainController*)sharedMainController {
  return base::mac::ObjCCast<MainApplicationDelegate>(
             [[UIApplication sharedApplication] delegate])
      .mainController;
}

@end
