// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_application_delegate.h"

#import <UserNotifications/UserNotifications.h>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/download/public/background_service/background_download_service.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/download/background_service/background_download_service_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/push_notification/push_notification_delegate.h"
#import "ios/chrome/browser/push_notification/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/keyboard/menu_builder.h"
#import "ios/web/common/uikit_ui_util.h"

namespace {
// The time delay after firstSceneWillEnterForeground: before checking for main
// intent signals.
const int kMainIntentCheckDelay = 1;
}  // namespace

@interface MainApplicationDelegate () <AppStateObserver> {
  MainController* _mainController;
  // Memory helper used to log the number of memory warnings received.
  MemoryWarningHelper* _memoryHelper;
  // Metrics mediator used to check and update the metrics accordingly to the
  // user preferences.
  MetricsMediator* _metricsMediator;
  // Container for startup information.
  id<StartupInformation> _startupInformation;
  // The set of "scene sessions" that needs to be discarded. See
  // -application:didDiscardSceneSessions: for details.
  NSSet<UISceneSession*>* _sceneSessionsToDiscard;
  // Delegate that handles delivered push notification workflow.
  PushNotificationDelegate* _pushNotificationDelegate;
  // YES if the application was able to successfully register itself with APNS
  // and obtain its APNS token.
  BOOL _didRegisterDeviceWithAPNS;
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
    _startupInformation = _mainController;
    _appState =
        [[AppState alloc] initWithStartupInformation:_startupInformation];
    _pushNotificationDelegate =
        [[PushNotificationDelegate alloc] initWithAppState:_appState];
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

  UNUserNotificationCenter* center =
      [UNUserNotificationCenter currentNotificationCenter];
  center.delegate = _pushNotificationDelegate;

  _appState.startupInformation.didFinishLaunchingTime = base::TimeTicks::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults
      setInteger:[defaults integerForKey:
                               metrics_mediator::
                                   kAppDidFinishLaunchingConsecutiveCallsKey] +
                 1
          forKey:metrics_mediator::kAppDidFinishLaunchingConsecutiveCallsKey];

  [_appState startInitialization];
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

  return YES;
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // Any report captured from this point on should be noted as after terminate.
  crash_keys::SetCrashedAfterAppWillTerminate();

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

- (void)application:(UIApplication*)application
    didReceiveRemoteNotification:(NSDictionary*)userInfo
          fetchCompletionHandler:
              (void (^)(UIBackgroundFetchResult result))completionHandler {
  // This method is invoked by iOS to process an incoming remote push
  // notification for the application and fetch any additional data.

  // According to the documentation, iOS invokes this function whether the
  // application is in the foreground or background. In addition, iOS will
  // launch the application and place it in background mode to invoke this
  // function. However, iOS will not do this if the user has force-quit the
  // application. In that case, the user must relaunch the application or must
  // restart the device before the system will launch the application and invoke
  // this function.
  UIBackgroundFetchResult result = [_pushNotificationDelegate
      applicationWillProcessIncomingRemoteNotification:userInfo];
  if (completionHandler) {
    completionHandler(result);
  }
}

- (void)application:(UIApplication*)application
    didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken {
  // This method is invoked by iOS on the successful registration of the app to
  // APNS and retrieval of the device's APNS token.
  _didRegisterDeviceWithAPNS = YES;
  base::UmaHistogramBoolean("IOS.PushNotification.APNSDeviceRegistration",
                            true);
  [_pushNotificationDelegate applicationDidRegisterWithAPNS:deviceToken];
}

- (void)application:(UIApplication*)application
    didFailToRegisterForRemoteNotificationsWithError:(NSError*)error {
  // This method is invoked by iOS to inform the application that the attempt to
  // obtain the device's APNS token from APNS failed
  base::UmaHistogramBoolean("IOS.PushNotification.APNSDeviceRegistration",
                            false);
}

- (void)application:(UIApplication*)application
    handleEventsForBackgroundURLSession:(NSString*)identifier
                      completionHandler:(void (^)())completionHandler {
  if (![identifier
          hasPrefix:base::SysUTF8ToNSString(
                        download::kBackgroundDownloadIdentifierPrefix)]) {
    completionHandler();
    return;
  }
  // TODO(crbug.com/1442203) Remove this Browser dependency, ideally by
  // refactoring into a dedicated agent.
  Browser* browser =
      _mainController.browserProviderInterface.mainBrowserProvider.browser;
  if (!browser) {
    // TODO(crbug.com/1368617): We should store the completionHandler and wait
    // for mainBrowserProvider creation.
    completionHandler();
    return;
  }
  download::BackgroundDownloadService* download_service =
      BackgroundDownloadServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  if (download_service) {
    download_service->HandleEventsForBackgroundURLSession(
        base::BindOnce(completionHandler));
    return;
  }
  completionHandler();
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
  // Reset `startupHadExternalIntent` for all scenes in case external intents
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
          base::UmaHistogramEnumeration(kAppLaunchSource,
                                        AppLaunchSource::APP_ICON);
          base::RecordAction(base::UserMetricsAction("IOSOpenByMainIntent"));
        } else {
          [self notifyFETAppStartupFromExternalIntent];
          base::RecordAction(base::UserMetricsAction("IOSOpenByViewIntent"));
        }
      });

  [self registerDeviceForPushNotifications];

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

#pragma mark - UIResponder methods

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [super buildMenuWithBuilder:builder];

  if (@available(iOS 15, *)) {
    [MenuBuilder buildMainMenuWithBuilder:builder];
  }
}

#pragma mark - Testing methods

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

- (MainController*)mainController {
  return _mainController;
}

#pragma mark - Private

// Registers the device with APNS to enable receiving push notifications to the
// device. In addition, the function sets the UNUserNotificationCenter's
// delegate which enables the application to display push notifications that
// were received while Chrome was open.
- (void)registerDeviceForPushNotifications {
  if (!_didRegisterDeviceWithAPNS && IsPriceNotificationsEnabled()) {
    [PushNotificationUtil registerDeviceWithAPNS];
  }
}

// Notifies the Feature Engagement Tracker (FET) that the app has launched from
// an external intent (i.e. through the share sheet), which is an eligibility
// criterion for the default browser blue dot promo.
// TODO(crbug.com/1442203) Remove this Browser dependency, ideally by
// refactoring into a dedicated agent.
- (void)notifyFETAppStartupFromExternalIntent {
  Browser* browser =
      _mainController.browserProviderInterface.mainBrowserProvider.browser;

  // OTR browsers are ignored because they can sometimes cause a nullptr tracker
  // to be returned from the tracker factory.
  if (!browser || browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          browser->GetBrowserState());

  tracker->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);

  tracker->NotifyEvent(
      feature_engagement::events::kDefaultBrowserVideoPromoConditionsMet);
}

@end
