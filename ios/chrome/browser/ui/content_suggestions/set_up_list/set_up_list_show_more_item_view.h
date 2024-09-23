// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

@class SetUpListItemViewData;
@protocol SetUpListTapDelegate;

// A view to display an individual Set Up List item in the Magic Stack.
@interface SetUpListShowMoreItemView : UIView

// Initialize a SetUpListShowMoreItemView with the given `data`.
- (instancetype)initWithData:(SetUpListItemViewData*)data;

// The object that should receive a message when this view is tapped.
@property(nonatomic, weak) id<SetUpListTapDelegate> tapDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_ITEM_VIEW_H_
