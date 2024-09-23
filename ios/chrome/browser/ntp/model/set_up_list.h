// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class PrefService;
@class SetUpListItem;
@protocol SetUpListDelegate;

namespace syncer {
class SyncService;
}  // namespace syncer

// Contains a list of items to display in the Set Up List UI on the NTP / Home.
@interface SetUpList : NSObject

// Builds a SetUpList instance, which includes a list of tasks the user hasn't
// completed yet. (For example: set Chrome as Default Browser). `prefs` are
// BrowserState prefs and are used to determine if CPE is enabled and
// `localState` is used to store each SetUpListItem's state.
// `authenticationService` is used to determine signed-in status. Returns `nil`
// if the Set Up List has been disabled in local state prefs.
// `contentNotificationEnabled` is `YES` if the user is enabled to content
// notifications.
+ (instancetype)buildFromPrefs:(PrefService*)prefs
                    localState:(PrefService*)localState
                   syncService:(syncer::SyncService*)syncService
         authenticationService:(AuthenticationService*)authService
    contentNotificationEnabled:(BOOL)isContentNotificationEnabled;

// Initializes a SetUpList with the given `items`. `localState` is used to
// store the state of each item and to observe changes to that state.
- (instancetype)initWithItems:(NSArray<SetUpListItem*>*)items
                    localState:(PrefService*)localState
         authenticationService:(AuthenticationService*)authService
    contentNotificationEnabled:(BOOL)isContentNotificationEnabled
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects and cleans up this Set Up List.
- (void)disconnect;

// Returns `YES` if all items are complete.
- (BOOL)allItemsComplete;

// Returns the complete list of tasks, inclusive of the ones the user has
// already completed.
- (NSArray<SetUpListItem*>*)allItems;

// Contains the items or tasks that the user may want to complete as part of
// setting up the app.
@property(nonatomic, strong, readonly) NSArray<SetUpListItem*>* items;

// Delegate to receive events from this Set Up List.
@property(nonatomic, weak) id<SetUpListDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_H_
