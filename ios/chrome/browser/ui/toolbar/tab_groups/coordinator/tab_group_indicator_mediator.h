// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_mutator.h"

@protocol TabGroupIndicatorConsumer;
@protocol TabGroupIndicatorCoordinatorDelegate;
class WebStateList;

// Mediator used to propagate tab group updates to the TabGroupIndicatorView.
@interface TabGroupIndicatorMediator : NSObject <TabGroupIndicatorMutator>

// Delegate for actions happening in the mediator.
@property(nonatomic, weak) id<TabGroupIndicatorCoordinatorDelegate> delegate;

// Creates an instance of the mediator.
- (instancetype)initWithConsumer:(id<TabGroupIndicatorConsumer>)consumer
                    webStateList:(WebStateList*)webStateList;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
