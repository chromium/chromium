// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"

@protocol ContentSuggestionsViewControllerAudience;
enum class SetUpListItemType;
@class SetUpListItemView;
@class SetUpListItemViewData;

// A protocol that would be implemented to handle taps on SetUpListItemViews.
@protocol SetUpListItemViewTapDelegate
// Indicates that the user has tapped the given `view`.
- (void)didTapSetUpListItemView:(SetUpListItemView*)view;
@end

// A view to display an individual item in the SetUpListView.
@interface SetUpListItemView : UIView <SetUpListConsumer>

// Initialize a SetUpListItemView with the given `data`.
- (instancetype)initWithData:(SetUpListItemViewData*)data;

// Indicates the type of item.
@property(nonatomic, readonly) SetUpListItemType type;

// Indicates whether this item is complete.
@property(nonatomic, readonly) BOOL complete;

// The object that should receive a message when this view is tapped.
@property(nonatomic, weak) id<SetUpListItemViewTapDelegate> tapDelegate;

// Command handler for this view's events.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    commandHandler;

// Marks this item as complete with an animation. When the animation is done,
// `completion` will be called.
- (void)markCompleteWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_H_
