// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/ui_bundled/scoped_notification_auth_swizzler.h"

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"

ScopedNotificationAuthSwizzler::ScopedNotificationAuthSwizzler(
    UNAuthorizationStatus initial_status,
    BOOL grant,
    UNNotificationSetting initial_lockscreen_setting,
    UNNotificationSetting initial_alert_setting) {
  status_ = initial_status;

  // Swizzle the authorization status.
  auto status_block = ^{
    return status_;
  };
  status_swizzler_ = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"UNNotificationSettings", @"authorizationStatus", status_block);

  // Swizzle the lockscreen setting.
  lockscreen_setting_ = initial_lockscreen_setting;
  auto lock_screen_block = ^{
    return lockscreen_setting_;
  };
  lockscreen_status_swizzler_ = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"UNNotificationSettings", @"lockScreenSetting", lock_screen_block);

  // Swizzle the alert setting.
  alert_setting_ = initial_alert_setting;
  auto alert_block = ^{
    return alert_setting_;
  };
  alert_status_swizzler_ = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"UNNotificationSettings", @"alertSetting", alert_block);

  // Swizzle the authorization request.
  auto request_block =
      ^(id center, UNAuthorizationOptions options,
        void (^completionHandler)(BOOL granted, NSError* error)) {
        if (grant) {
          // If app asked for provisional, grant provisional.
          status_ = options & UNAuthorizationOptionProvisional
                        ? UNAuthorizationStatusProvisional
                        : UNAuthorizationStatusAuthorized;
        } else {
          status_ = UNAuthorizationStatusDenied;
        }
        completionHandler(grant, nil);
      };
  request_swizzler_ = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"UNUserNotificationCenter",
      @"requestAuthorizationWithOptions:completionHandler:", request_block);
}

ScopedNotificationAuthSwizzler::ScopedNotificationAuthSwizzler(BOOL grant)
    : ScopedNotificationAuthSwizzler(UNAuthorizationStatusNotDetermined,
                                     grant) {}

ScopedNotificationAuthSwizzler::~ScopedNotificationAuthSwizzler() {}
