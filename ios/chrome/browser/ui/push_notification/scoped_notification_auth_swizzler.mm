// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/ui/push_notification/scoped_notification_auth_swizzler.h"

#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"

ScopedNotificationAuthSwizzler::ScopedNotificationAuthSwizzler(
    UNAuthorizationStatus initial_status,
    BOOL grant) {
  status_ = initial_status;

  // Swizzle the authorization status.
  auto status_block = ^{
    return status_;
  };
  status_swizzler_ = std::make_unique<EarlGreyScopedBlockSwizzler>(
      @"UNNotificationSettings", @"authorizationStatus", status_block);

  // Swizzle the authorization request.
  auto request_block =
      ^(id center, UNAuthorizationOptions options,
        void (^completionHandler)(BOOL granted, NSError* error)) {
        status_ = grant ? UNAuthorizationStatusAuthorized
                        : UNAuthorizationStatusDenied;
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
