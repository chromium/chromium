// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/ui/tab_resumption_consumer.h"

@protocol TabResumptionCommands;
@protocol TabResumptionConsumer;
@class TabResumptionItem;

// A view that displays a tab resumption item in the Magic Stack.
@interface TabResumptionView : UIView <TabResumptionConsumer>

// Initialize a TabResumptionView with the given `item`.
- (instancetype)initWithItem:(TabResumptionItem*)item;

// The handler that receives TabResumptionView's events.
@property(nonatomic, weak) id<TabResumptionCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_
