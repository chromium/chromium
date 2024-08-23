// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/send_tab_to_self/features.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/content_notification/model/content_notification_settings_action.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {
// The time range's expected min and max values for custom histograms.
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMin =
    base::Milliseconds(1);
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMax =
    base::Seconds(30);
// Number of buckets for the time range histograms.
constexpr int kTimeRangeHistogramBucketCount = 30;

// The histogram used to record a push notification's current lifecycle state on
// the device.
const char kLifecycleEventsHistogram[] = "IOS.PushNotification.LifecyleEvents";

// This enum is used to represent a point along the push notification's
// lifecycle.
enum class PushNotificationLifecycleEvent {
  kNotificationReception,
  kNotificationForegroundPresentation,
  kNotificationInteraction,
  kMaxValue = kNotificationInteraction
};

// This function creates a dictionary that maps signed-in user's GAIA IDs to a
// map of each user's preferences for each push notification enabled feature.
GaiaIdToPushNotificationPreferenceMap*
GaiaIdToPushNotificationPreferenceMapFromCache(
    BrowserStateInfoCache* info_cache) {
  size_t number_of_browser_states = info_cache->GetNumberOfBrowserStates();
  NSMutableDictionary* account_preference_map =
      [[NSMutableDictionary alloc] init];

  for (size_t i = 0; i < number_of_browser_states; i++) {
    NSString* gaia_id =
        base::SysUTF8ToNSString(info_cache->GetGAIAIdOfBrowserStateAtIndex(i));
    if (!gaia_id.length) {
      continue;
    }

    base::FilePath path = info_cache->GetPathOfBrowserStateAtIndex(i);
    PrefService* pref_service = GetApplicationContext()
                                    ->GetChromeBrowserStateManager()
                                    ->GetBrowserStateByPath(path)
                                    ->GetPrefs();

    NSMutableDictionary<NSString*, NSNumber*>* preference_map =
        [[NSMutableDictionary alloc] init];
    const base::Value::Dict& permissions =
        pref_service->GetDict(prefs::kFeaturePushNotificationPermissions);

    for (const auto pair : permissions) {
      preference_map[base::SysUTF8ToNSString(pair.first)] =
          [NSNumber numberWithBool:pair.second.GetBool()];
    }

    account_preference_map[gaia_id] = preference_map;
  }

  return account_preference_map;
}

}  // anonymous namespace

@implementation PushNotificationDelegate

- (instancetype)initWithAppState:(AppState*)appState {
  [appState addObserver:self];
  return self;
}

#pragma mark - UNUserNotificationCenterDelegate -

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationInteraction];
  // This method is invoked by iOS to process the user's response to a delivered
  // notification.
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->HandleNotificationInteraction(response);
  if (completionHandler) {
    completionHandler();
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationForegroundPresentation];
  // This method is invoked by iOS to process a notification that arrived while
  // the app was running in the foreground.
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->HandleNotificationReception(
      notification.request.content.userInfo);

  if (completionHandler) {
    completionHandler(UNNotificationPresentationOptionBanner);
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

#pragma mark - PushNotificationDelegate

- (UIBackgroundFetchResult)applicationWillProcessIncomingRemoteNotification:
    (NSDictionary*)userInfo {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationReception];

  double incomingNotificationTime =
      base::Time::Now().InSecondsFSinceUnixEpoch();
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  UIBackgroundFetchResult result =
      clientManager->HandleNotificationReception(userInfo);

  double processingTime =
      base::Time::Now().InSecondsFSinceUnixEpoch() - incomingNotificationTime;
  UmaHistogramCustomTimes(
      "IOS.PushNotification.IncomingNotificationProcessingTime",
      base::Milliseconds(processingTime),
      kTimeRangeIncomingNotificationHistogramMin,
      kTimeRangeIncomingNotificationHistogramMax,
      kTimeRangeHistogramBucketCount);
  return result;
}

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken
                          browserState:(ChromeBrowserState*)browserState {
  BrowserStateInfoCache* infoCache = GetApplicationContext()
                                         ->GetChromeBrowserStateManager()
                                         ->GetBrowserStateInfoCache();

  GaiaIdToPushNotificationPreferenceMap* accountPreferenceMap =
      GaiaIdToPushNotificationPreferenceMapFromCache(infoCache);

  // Return early if no accounts are signed into Chrome.
  if (!accountPreferenceMap.count) {
    return;
  }

  PushNotificationService* notificationService =
      GetApplicationContext()->GetPushNotificationService();

  // Registers Chrome's PushNotificationClients' Actionable Notifications with
  // iOS.
  notificationService->GetPushNotificationClientManager()
      ->RegisterActionableNotifications();

  PushNotificationConfiguration* config =
      [[PushNotificationConfiguration alloc] init];

  config.accountIDs = accountPreferenceMap.allKeys;
  config.preferenceMap = accountPreferenceMap;
  config.deviceToken = deviceToken;
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();

  if (browserState) {
    config.shouldRegisterContentNotification =
        [self isContentNotificationAvailable:browserState];
    if (config.shouldRegisterContentNotification) {
      AuthenticationService* authService =
          AuthenticationServiceFactory::GetForBrowserState(browserState);
      id<SystemIdentity> identity =
          authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      config.primaryAccount = identity;
    }
  }

  notificationService->RegisterDevice(config, ^(NSError* error) {
    if (error) {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                false);
    } else {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                true);
      if (base::FeatureList::IsEnabled(
              send_tab_to_self::kSendTabToSelfIOSPushNotifications) &&
          browserState) {
        DeviceInfoSyncServiceFactory::GetForBrowserState(browserState)
            ->RefreshLocalDeviceInfo();
      }
    }
  });
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (appState.initStage < InitStageFinal) {
    return;
  }
  SceneState* sceneState = appState.foregroundActiveScene;
  if (sceneState == nil) {
    return;
  }
  [self appDidEnterForeground:sceneState];
}

- (void)appState:(AppState*)appState
    sceneDidBecomeActive:(SceneState*)sceneState {
  if (appState.initStage < InitStageFinal) {
    return;
  }
  [self appDidEnterForeground:sceneState];
}

#pragma mark - Private

// Notifies the client manager that the scene is "foreground active".
- (void)appDidEnterForeground:(SceneState*)sceneState {
  PushNotificationClientManager* clientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->OnSceneActiveForegroundBrowserReady();
  ChromeBrowserState* browserState =
      sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState();
  if (IsContentNotificationEnabled(browserState)) {
    ContentNotificationService* contentNotificationService =
        ContentNotificationServiceFactory::GetForBrowserState(browserState);
    int maxNauSentPerSession = base::GetFieldTrialParamByFeatureAsInt(
        kContentNotificationDeliveredNAU, kDeliveredNAUMaxPerSession,
        kDeliveredNAUMaxSendsPerSession);
    // Check if there are notifications received in the background to send the
    // respective NAUs.
    NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
    if ([defaults objectForKey:kContentNotificationContentArrayKey] != nil) {
      NSMutableArray* contentArray = [[defaults
          objectForKey:kContentNotificationContentArrayKey] mutableCopy];
      // Report in 5 item increments.
      NSMutableArray* uploadedItems = [NSMutableArray array];
      for (NSData* item in contentArray) {
        ContentNotificationNAUConfiguration* config =
            [[ContentNotificationNAUConfiguration alloc] init];
        config.actionType = NAUActionTypeDisplayed;
        UNNotificationContent* content = [NSKeyedUnarchiver
            unarchivedObjectOfClass:UNMutableNotificationContent.class
                           fromData:item
                              error:nil];
        config.content = content;
        contentNotificationService->SendNAUForConfiguration(config);
        [uploadedItems addObject:item];
        base::UmaHistogramEnumeration(
            kContentNotificationActionHistogramName,
            NotificationActionType::kNotificationActionTypeDisplayed);
        if ((int)uploadedItems.count == maxNauSentPerSession) {
          break;
        }
      }
      [contentArray removeObjectsInArray:uploadedItems];
      if (contentArray.count > 0) {
        [defaults setObject:contentArray
                     forKey:kContentNotificationContentArrayKey];
      } else {
        [defaults setObject:nil forKey:kContentNotificationContentArrayKey];
      }
    }
    // Send an NAU on every foreground to report the OS Auth Settings.
    [PushNotificationUtil
        getPermissionSettings:^(UNNotificationSettings* settings) {
          UNAuthorizationStatus previousAuthStatus =
              [PushNotificationUtil getSavedPermissionSettings];
            ContentNotificationNAUConfiguration* config =
                [[ContentNotificationNAUConfiguration alloc] init];
            ContentNotificationSettingsAction* settingsAction =
                [[ContentNotificationSettingsAction alloc] init];
            settingsAction.previousAuthorizationStatus = previousAuthStatus;
            settingsAction.currentAuthorizationStatus =
                settings.authorizationStatus;
            config.settingsAction = settingsAction;
            contentNotificationService->SendNAUForConfiguration(config);
        }];
  }
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        [PushNotificationUtil
            updateAuthorizationStatusPref:settings.authorizationStatus];
      }];
}

- (void)recordLifeCycleEvent:(PushNotificationLifecycleEvent)event {
  base::UmaHistogramEnumeration(kLifecycleEventsHistogram, event);
}

- (BOOL)isContentNotificationAvailable:(ChromeBrowserState*)browserState {
  return IsContentNotificationEnabled(browserState) ||
         IsContentNotificationRegistered(browserState);
}

@end
