// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@protocol SingleSignOnService;

// This class is intended to store the information needed to initialize and
// register the device to receive push notifications from the push notification
// server.
@interface PushNotificationConfiguration : NSObject

// A list of the application's currently logged in users' GAIA (Google Accounts
// ID Administration) IDs.
@property(nonatomic) NSArray<NSString*>* accountIDs;

// An Apple Push Notification Service (APNS) device token. The device token is
// used to associate the `accountIDs` to the device.
@property(nonatomic) NSData* deviceToken;

// SingleSignOnService used by PushNotificationService.
@property(nonatomic) id<SingleSignOnService> ssoService;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CONFIGURATION_H_