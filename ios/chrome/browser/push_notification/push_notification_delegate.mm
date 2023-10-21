// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_delegate.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/push_notification_delegate.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

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
                                    ->GetBrowserState(path)
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

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken {
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
  config.ssoService = GetApplicationContext()->GetSSOService();

  notificationService->RegisterDevice(config, ^(NSError* error) {
    if (error) {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                false);
    } else {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                true);
    }
  });
}

#pragma mark - AppStateObserver
- (void)appState:(AppState*)appState
    sceneDidBecomeActive:(SceneState*)sceneState {
  if (appState.initStage < InitStageFinal) {
    return;
  }
  PushNotificationClientManager* clientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->OnSceneActiveForegroundBrowserReady();
}

#pragma mark - Private
- (void)recordLifeCycleEvent:(PushNotificationLifecycleEvent)event {
  base::UmaHistogramEnumeration(kLifecycleEventsHistogram, event);
}

@end
