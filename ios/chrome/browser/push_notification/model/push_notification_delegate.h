// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_DELEGATE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@interface PushNotificationDelegate
    : NSObject <UNUserNotificationCenterDelegate, AppStateObserver>

- (instancetype)initWithAppState:(AppState*)appState NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Passes the contents of an incoming push notification to the appropriate
// `PushNotificationClient` for processing and logs the time it takes for the
// client to process the notification.
- (UIBackgroundFetchResult)applicationWillProcessIncomingRemoteNotification:
    (NSDictionary*)userInfo;

// When the device successfully registers with APNS and receives its APNS device
// token this function aggregates all the necessary information and registers
// the device to the Push Notification server.
- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken
                               profile:(ProfileIOS*)profile;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_DELEGATE_H_
