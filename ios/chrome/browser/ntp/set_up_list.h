// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SET_UP_LIST_H_
#define IOS_CHROME_BROWSER_NTP_SET_UP_LIST_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class PrefService;
@class SetUpListItem;

// Contains a list of items to display in the Set Up List UI on the NTP / Home.
@interface SetUpList : NSObject

// Initializes a SetUpList with the given `items`.
- (instancetype)initWithItems:(NSArray<SetUpListItem*>*)items
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Builds a SetUpList instance, which includes a list of tasks the user hasn't
// completed yet. (For example: set Chrome as Default Browser).
+ (instancetype)buildFromPrefs:(PrefService*)prefs
         authenticationService:(AuthenticationService*)authService;

// Contains the items or tasks that the user may want to complete as part of
// setting up the app.
@property(nonatomic, strong, readonly) NSArray<SetUpListItem*>* items;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SET_UP_LIST_H_
