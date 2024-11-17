// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

enum class PushNotificationClientId;

// The purpose of this class is to manage the mapping between GaiaIDs and its
// context data related to push notifications.
@interface PushNotificationAccountContextManager : NSObject

// The designated initializer. `manager` must not be nil.
- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds the account to the manager if the account is not signed into the device
// in any Profile. This function returns a BOOL value indicating whether
// the account was added to the manager.
- (BOOL)addAccount:(const std::string&)gaiaID;

// Removes the account from the manager if the account is not signed into the
// device in any Profile. This function returns a BOOL value indicating
// whether the account was removed from the manager.
- (BOOL)removeAccount:(const std::string&)gaiaID;

// Enables the user with the given `gaiaID` to begin receiving push
// notifications from `clientID`.
- (void)enablePushNotification:(PushNotificationClientId)clientID
                    forAccount:(const std::string&)gaiaID;

// Prevents the user with the given `gaiaID` from receiving push notifications
// from `clientID`.
- (void)disablePushNotification:(PushNotificationClientId)clientID
                     forAccount:(const std::string&)gaiaID;

// Returns the user with the given `gaiaID` can receive push notifications from
// `clientID`.
- (BOOL)isPushNotificationEnabledForClient:(PushNotificationClientId)clientID
                                forAccount:(const std::string&)gaiaID;

// Returns a dictionary that maps PushNotificationClientIDs, stored as
// NSString, to a boolean value, stored as NSNumber, indicating whether the
// client has push notification permission for the given user.
- (NSDictionary<NSString*, NSNumber*>*)preferenceMapForAccount:
    (const std::string&)gaiaID;

// Returns a list of GAIA IDs registered with context manager.
- (NSArray<NSString*>*)accountIDs;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_
