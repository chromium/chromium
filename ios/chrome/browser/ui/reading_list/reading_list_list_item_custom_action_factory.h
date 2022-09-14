// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_CUSTOM_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_CUSTOM_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

@protocol ReadingListListItem;
@protocol ReadingListListItemAccessibilityDelegate;
@protocol ReadingListListItemFactoryDelegate;

// Factory object that creates arrays of custom accessibility actions for
// ListItems used by the reading list.
@interface ReadingListListItemCustomActionFactory : NSObject

// Delegate for the accessibility actions.
@property(nonatomic, weak) id<ReadingListListItemAccessibilityDelegate>
    accessibilityDelegate;

// Delegate for the incognito avaibility.
@property(nonatomic, weak) id<ReadingListListItemFactoryDelegate>
    incognitoDelegate;

// Creates an array of custom a11y actions for a reading list cell configured
// for `item` with `status`.
- (NSArray<UIAccessibilityCustomAction*>*)customActionsForItem:
    (id<ReadingListListItem>)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_CUSTOM_ACTION_FACTORY_H_
