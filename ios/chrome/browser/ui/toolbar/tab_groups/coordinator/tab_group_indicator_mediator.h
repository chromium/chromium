// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol TabGroupIndicatorConsumer;
class WebStateList;

// Mediator used to propagate tab group updates to the TabGroupIndicatorView.
@interface TabGroupIndicatorMediator : NSObject

// Creates an instance of the mediator.
- (instancetype)initWithConsumer:(id<TabGroupIndicatorConsumer>)consumer
                    webStateList:(WebStateList*)webStateList;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
