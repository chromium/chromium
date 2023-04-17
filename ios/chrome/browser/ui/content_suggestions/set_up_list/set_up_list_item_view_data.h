// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_DATA_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_DATA_H_

#import <UIKit/UIKit.h>

enum class SetUpListItemType;

// A view to display an individual item in the SetUpListView.
@interface SetUpListItemViewData : NSObject

// Initialize a SetUpListItemView with the given `type` and `complete` state.
- (instancetype)initWithType:(SetUpListItemType)type complete:(BOOL)complete;

// Indicates the type of item.
@property(nonatomic, readonly) SetUpListItemType type;

// Indicates whether this item is complete.
@property(nonatomic, readonly) BOOL complete;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_DATA_H_
