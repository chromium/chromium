// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_VIEW_H_

#import <UIKit/UIKit.h>

@class TabResumptionItem;
@protocol TabResumptionCommands;

// A view that displays a tab resumption item in the Magic Stack.
@interface TabResumptionView : UIView

// Initialize a TabResumptionView with the given `item`.
- (instancetype)initWithItem:(TabResumptionItem*)item;

// The handler that receives TabResumptionView's events.
@property(nonatomic, weak) id<TabResumptionCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_VIEW_H_
