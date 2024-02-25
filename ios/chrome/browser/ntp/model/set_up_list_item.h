// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_H_

#import <UIKit/UIKit.h>

enum class SetUpListItemType;

// An item (or task) that might appear in the Set Up List on the NTP / Home.
@interface SetUpListItem : NSObject

// Initializes a SetUpListItem with the given `type` and `complete` state.
- (instancetype)initWithType:(SetUpListItemType)type
                    complete:(BOOL)complete NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The item type for this item.
@property(nonatomic, assign, readonly) SetUpListItemType type;

// Whether this item is complete.
@property(nonatomic, assign, readonly) BOOL complete;

// Marks the item as completed.
- (void)markComplete;

@end

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_H_
