// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"

class BrowserStateInfoCache;

namespace ios {
class ChromeBrowserStateManager;

}

// This class is intended to store the permissions for each push notification
// enabled feature for a given account and the number of times the account is
// signed in across BrowserStates.
@interface PushNotificationAccountContext : NSObject
// A dictionary that maps the string value of a push notification client id to
// the perf service value for that push notification enable feature.
@property(nonatomic, readonly)
    NSDictionary<NSString*, NSNumber*>* preferenceMap;
// A counter that stores the number of times a given account is used across
// BrowserStates.
@property(nonatomic, readonly) NSUInteger occurrencesAcrossBrowserStates;
@end

// The purpose of this class is to manage the mapping between GaiaIDs and its
// context data related to push notifications.
@interface PushNotificationAccountContextManager : NSObject

// The designated initializer. `BrowserStateInfoCache` must not be nil.
- (instancetype)initWithChromeBrowserStateManager:
    (ios::ChromeBrowserStateManager*)manager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds the account to the manager if the account is not signed into the device
// in any BrowserState. This function returns a BOOL value indicating whether
// the account was added to the manager.
- (BOOL)addAccount:(NSString*)gaiaID;

// Removes the account from the manager if the account is not signed into the
// device in any BrowserState. This function returns a BOOL value indicating
// whether the account was removed from the manager.
- (BOOL)removeAccount:(NSString*)gaiaID;

// A dictionary that maps a user's GAIA ID to an object containing the account's
// preferences for all push notification enabled features and an number
// representing the number of times the account is signed in across
// BrowserStates.
@property(nonatomic, readonly)
    NSDictionary<NSString*, PushNotificationAccountContext*>* contextMap;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_H_
