// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_delegate.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/push_notification_delegate.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The time range's expected min and max values for custom histograms.
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMin =
    base::Milliseconds(1);
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMax =
    base::Seconds(30);
// Number of buckets for the time range histograms.
constexpr int kTimeRangeHistogramBucketCount = 30;
}  // namespace

@implementation PushNotificationDelegate

#pragma mark - UNUserNotificationCenterDelegate -

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
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
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
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
}

#pragma mark - PushNotificationDelegate

- (UIBackgroundFetchResult)applicationWillProcessIncomingRemoteNotification:
    (NSDictionary*)userInfo {
  double incomingNotificationTime = base::Time::Now().ToDoubleT();
  base::UmaHistogramBoolean("IOS.PushNotification.APNSDeviceRegistration",
                            true);
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  UIBackgroundFetchResult result =
      clientManager->HandleNotificationReception(userInfo);

  double processingTime =
      incomingNotificationTime - base::Time::Now().ToDoubleT();
  UmaHistogramCustomTimes(
      "IOS.PushNotification.IncomingNotificationProcessingTime",
      base::Milliseconds(processingTime),
      kTimeRangeIncomingNotificationHistogramMin,
      kTimeRangeIncomingNotificationHistogramMax,
      kTimeRangeHistogramBucketCount);
  return result;
}

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken {
  BrowserStateInfoCache* info_cache = GetApplicationContext()
                                          ->GetChromeBrowserStateManager()
                                          ->GetBrowserStateInfoCache();

  size_t number_of_browser_states = info_cache->GetNumberOfBrowserStates();
  NSMutableArray* account_ids = [[NSMutableArray alloc] init];

  for (size_t i = 0; i < number_of_browser_states; i++) {
    NSString* gaia_id =
        base::SysUTF8ToNSString(info_cache->GetGAIAIdOfBrowserStateAtIndex(i));
    if (![gaia_id isEqualToString:@""]) {
      [account_ids addObject:gaia_id];
    }
  }

  // Return early if no accounts are signed into Chrome.
  if (![account_ids count]) {
    return;
  }

  PushNotificationService* notification_service =
      GetApplicationContext()->GetPushNotificationService();

  PushNotificationConfiguration* config =
      [[PushNotificationConfiguration alloc] init];

  config.accountIDs = account_ids;
  config.deviceToken = deviceToken;
  config.ssoService = GetApplicationContext()->GetSSOService();

  notification_service->RegisterDevice(config, ^(NSError* error) {
    if (error) {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                false);
    } else {
      base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                                true);
    }
  });
}

@end