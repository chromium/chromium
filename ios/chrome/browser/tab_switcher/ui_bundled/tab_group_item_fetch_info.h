// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_FETCH_INFO_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_FETCH_INFO_H_

#import <Foundation/Foundation.h>

// Model object used to fetch favicons and snapshots of a group item.
// Stores the request ID and a counter for remaining sub-fetches.
@interface TabGroupItemFetchInfo : NSObject

// Unique ID for this fetch request.
@property(nonatomic, strong, readonly) NSUUID* requestID;

// Initializes with a request ID and the total expected fetches.
// requestID: The unique ID for this fetch.
// initialCount: The starting count of sub-fetches.
- (instancetype)initWithRequestID:(NSUUID*)requestID
                initialFetchCount:(NSInteger)initialCount
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Decrements the counter and returns the new value.
- (NSInteger)decrementRemainingFetches;

// Reads the current remaining fetch count.
- (NSInteger)currentRemainingFetches;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_FETCH_INFO_H_
