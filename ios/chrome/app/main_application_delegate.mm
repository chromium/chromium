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
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The time delay after firstSceneWillEnterForeground: before checking for main
// intent signals.
const int kMainIntentCheckDelay = 1;
}  // namespace

@interface MainApplicationDelegate () {
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
  id<TabSwitching> _tabSwitcherProtocol;
}

// The state representing the only "scene" on iOS 12. On iOS 13, only created
// temporarily before multiwindow is fully implemented to also represent the
// only scene.
@property(nonatomic, strong) SceneState* sceneState;

// The controller for |sceneState|.
@property(nonatomic, strong) SceneController* sceneController;

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

    if (!base::ios::IsSceneStartupSupported()) {
      // When the UIScene APU is not supported, this object holds a "scene"
      // state and a "scene" controller. This allows the rest of the app to be
      // mostly multiwindow-agnostic.
      _sceneState = [[SceneState alloc] initWithAppState:_appState];
      _appState.mainSceneState = _sceneState;
      _sceneController =
          [[SceneController alloc] initWithSceneState:_sceneState];
      _sceneState.controller = _sceneController;

      _tabSwitcherProtocol = _sceneController;
      _tabOpener = _sceneController;
    }
  }
  return self;
}

- (UIWindow*)window {
  return self.sceneState.window;
}

- (void)setWindow:(UIWindow*)newWindow {
  NOTREACHED() << "Should not be called, use [SceneState window] instead";
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

  _mainController.window = self.window;

  BOOL inBackground =
      [application applicationState] == UIApplicationStateBackground;
  BOOL requiresHandling =
      [_appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                        stateBackground:inBackground];
  if (!base::ios::IsSceneStartupSupported()) {
    self.sceneState.activationLevel =
        inBackground ? SceneActivationLevelBackground
                     : SceneActivationLevelForegroundInactive;
  }

  if (@available(iOS 13, *)) {
    if (base::ios::IsSceneStartupSupported()) {
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(sceneWillConnect:)
                 name:UISceneWillConnectNotification
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
    }
  }

  return requiresHandling;
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  if (!base::ios::IsSceneStartupSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundActive;
  }

  if (_appState.initStage <= InitStageSafeMode)
    return;

  if (!base::ios::IsSceneStartupSupported()) {
    [_appState resumeSessionWithTabOpener:_tabOpener
                              tabSwitcher:_tabSwitcherProtocol
                    connectionInformation:self.sceneController];
  }
}

- (void)applicationWillResignActive:(UIApplication*)application {
  if (!base::ios::IsSceneStartupSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
  }

  if (_appState.initStage <= InitStageSafeMode)
    return;

  [_appState willResignActiveTabModel];
}

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application {
  if (!base::ios::IsSceneStartupSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelBackground;
  }

  [_appState applicationDidEnterBackground:application
                              memoryHelper:_memoryHelper];
}

// Called when returning to the foreground.
- (void)applicationWillEnterForeground:(UIApplication*)application {
  if (!base::ios::IsSceneStartupSupported()) {
    self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
  }

  [_appState applicationWillEnterForeground:application
                            metricsMediator:_metricsMediator
                               memoryHelper:_memoryHelper];
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // If |self.didFinishLaunching| is NO, that indicates that the app was
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
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions
    API_AVAILABLE(ios(13)) {
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->ApplicationDidDiscardSceneSessions(sceneSessions);
  [_appState application:application didDiscardSceneSessions:sceneSessions];
}

#pragma mark - Scenes lifecycle

- (NSInteger)foregroundSceneCount {
  DCHECK(base::ios::IsSceneStartupSupported());
  if (@available(iOS 13, *)) {
    NSInteger foregroundSceneCount = 0;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
      if ((scene.activationState == UISceneActivationStateForegroundInactive) ||
          (scene.activationState == UISceneActivationStateForegroundActive)) {
        foregroundSceneCount++;
      }
    }
    return foregroundSceneCount;
  }
  return 0;
}

- (void)sceneWillConnect:(NSNotification*)notification {
  DCHECK(base::ios::IsSceneStartupSupported());
  if (@available(iOS 13, *)) {
    UIWindowScene* scene = (UIWindowScene*)notification.object;
    SceneDelegate* sceneDelegate = (SceneDelegate*)scene.delegate;
    SceneController* sceneController = sceneDelegate.sceneController;

    _tabSwitcherProtocol = sceneController;
    _tabOpener = sceneController;

    // TODO(crbug.com/1060645): This should be called later, or this flow should
    // be changed completely.
    if (self.foregroundSceneCount == 0) {
      [_appState applicationWillEnterForeground:UIApplication.sharedApplication
                                metricsMediator:_metricsMediator
                                   memoryHelper:_memoryHelper];
    }
  }
}

- (void)lastSceneDidEnterBackground:(NSNotification*)notification {
  DCHECK(base::ios::IsSceneStartupSupported());
  // Reset |startupHadExternalIntent| for all Scenes in case external intents
  // were triggered while the application was in the foreground.
  for (SceneState* scene in self.appState.connectedScenes) {
    if (scene.startupHadExternalIntent) {
      scene.startupHadExternalIntent = NO;
    }
  }
  if (@available(iOS 13, *)) {
    [_appState applicationDidEnterBackground:UIApplication.sharedApplication
                                memoryHelper:_memoryHelper];
  }
}

- (void)firstSceneWillEnterForeground:(NSNotification*)notification {
  DCHECK(base::ios::IsSceneStartupSupported());
  if (@available(iOS 13, *)) {
    __weak MainApplicationDelegate* weakSelf = self;
    // Delay Main Intent check since signals for intents like spotlight actions
    // are not guaranteed to occur before firstSceneWillEnterForeground.
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
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
            base::RecordAction(
                base::UserMetricsAction("IOSOpenByMainIntent"));
          }
        });
    [_appState applicationWillEnterForeground:UIApplication.sharedApplication
                              metricsMediator:_metricsMediator
                                 memoryHelper:_memoryHelper];
  }
}

#pragma mark Downloading Data in the Background

- (void)application:(UIApplication*)application
    handleEventsForBackgroundURLSession:(NSString*)identifier
                      completionHandler:(void (^)(void))completionHandler {
  completionHandler();
}

#pragma mark Continuing User Activity and Handling Quick Actions

- (BOOL)application:(UIApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  if (_appState.initStage <= InitStageSafeMode)
    return NO;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  return
      [UserActivityHandler willContinueUserActivityWithType:userActivityType];
}

- (BOOL)application:(UIApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:
          (void (^)(NSArray<id<UIUserActivityRestoring>>*))restorationHandler {
  if (_appState.initStage <= InitStageSafeMode)
    return NO;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  BOOL applicationIsActive =
      [application applicationState] == UIApplicationStateActive;

  return [UserActivityHandler
       continueUserActivity:userActivity
        applicationIsActive:applicationIsActive
                  tabOpener:_tabOpener
      connectionInformation:self.sceneController
         startupInformation:_startupInformation
               browserState:_mainController.interfaceProvider.currentInterface
                                .browserState];
}

- (void)application:(UIApplication*)application
    performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
               completionHandler:(void (^)(BOOL succeeded))completionHandler {
  if (_appState.initStage <= InitStageSafeMode)
    return;

  // Enusre Chrome is fuilly started up in case it had launched to the
  // background.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  [UserActivityHandler
      performActionForShortcutItem:shortcutItem
                 completionHandler:completionHandler
                         tabOpener:_tabOpener
             connectionInformation:self.sceneController
                startupInformation:_startupInformation
                 interfaceProvider:_mainController.interfaceProvider];
}

#pragma mark Opening a URL-Specified Resource

// Handles open URL. The registered URL Schemes are defined in project
// variables ${CHROMIUM_URL_SCHEME_x}.
// The url can either be empty, in which case the app is simply opened or
// can contain an URL that will be opened in a new tab.
- (BOOL)application:(UIApplication*)application
            openURL:(NSURL*)url
            options:(NSDictionary<NSString*, id>*)options {
  if (_appState.initStage <= InitStageSafeMode)
    return NO;

  // The various URL handling mechanisms require that the application has
  // fully started up; there are some cases (crbug.com/658420) where a
  // launch via this method crashes because some services (specifically,
  // CommandLine) aren't initialized yet. So: before anything further is
  // done, make sure that Chrome is fully started up.
  [_browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  if (ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->HandleApplicationOpenURL(application, url, options)) {
    return YES;
  }

  BOOL applicationActive =
      [application applicationState] == UIApplicationStateActive;

  return [URLOpener openURL:[[URLOpenerParams alloc] initWithOpenURL:url
                                                             options:options]
          applicationActive:applicationActive
                  tabOpener:_tabOpener
      connectionInformation:self.sceneController
         startupInformation:_startupInformation
                prefService:_mainController.interfaceProvider.currentInterface
                                .browserState->GetPrefs()];
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
