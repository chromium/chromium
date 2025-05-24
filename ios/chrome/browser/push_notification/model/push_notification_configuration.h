// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class PushNotificationAccountContextManager;
@protocol SingleSignOnService;
@protocol SystemIdentity;

using GaiaIdToPushNotificationPreferenceMap =
    NSDictionary<NSString*, NSDictionary<NSString*, NSNumber*>*>;

// This class is intended to store the information needed to initialize and
// register the device to receive push notifications from the push notification
// server.
@interface PushNotificationConfiguration : NSObject
// DEPRECATED. `accountIDs` is deprecated in favor of `preferenceMap`. A list of
// the application's currently logged in users' GAIA (Google Accounts ID
// Administration) IDs.
@property(nonatomic, copy) NSArray<NSString*>* accountIDs;

// An Apple Push Notification Service (APNS) device token. The device token is
// used to associate the `accountIDs` to the device.
@property(nonatomic, strong) NSData* deviceToken;

// SingleSignOnService used by PushNotificationService.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

// DEPRECATED. Please use the `contextManager.contextMap` instead. A dictionary
// that maps a user's GAIA ID to its preferences for all push notification
// enabled features.
@property(nonatomic, copy) GaiaIdToPushNotificationPreferenceMap* preferenceMap;

// A dictionary that maps a user's GAIA ID to an object containing the account's
// preferences for all push notification enabled features and an number
// representing the number of times the account is signed in across
// Profiles.
@property(nonatomic, strong)
    PushNotificationAccountContextManager* contextManager;

// The primary account's identity. Used for Content Notification registration.
@property(nonatomic, strong) id<SystemIdentity> primaryAccount;

// `YES` if the user should be registered to Content Notification.
@property(nonatomic, assign) BOOL shouldRegisterContentNotification;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CONFIGURATION_H_
