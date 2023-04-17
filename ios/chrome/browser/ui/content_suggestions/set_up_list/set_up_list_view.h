// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_VIEW_H_

#import <UIKit/UIKit.h>

enum class SetUpListItemType;
@class SetUpListItemViewData;
@protocol SetUpListCommands;

// A delegate protocol to be implemented by the owner of the SetUpListView.
@protocol SetUpListViewDelegate <NSObject>

// Called when a Set Up List item is selected by the user.
- (void)didSelectSetUpListItem:(SetUpListItemType)type;

// Called when the user selects the Set Up List menu.
- (void)showSetUpListMenu;

@end

// A view that displays the Set Up List, a list of tasks a new user may want
// to complete to set up the app.
@interface SetUpListView : UIView

// Initializes the SetUpListView, with the given items.
- (instancetype)initWithItems:(NSArray<SetUpListItemViewData*>*)items;

// The object that should handle delegate events.
@property(nonatomic, weak) id<SetUpListViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_VIEW_H_
