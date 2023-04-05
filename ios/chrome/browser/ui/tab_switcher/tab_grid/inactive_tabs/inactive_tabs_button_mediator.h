// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol InactiveTabsInfoConsumer;
class PrefService;
class WebStateList;

// This mediator updates the button in the regular Tab Grid showing the presence
// and number of inactive tabs.
@interface InactiveTabsButtonMediator : NSObject

// Initializer with `consumer` as the receiver of Inactive Tabs info updates
// (count of inactive tabs, user setting).
- (instancetype)initWithConsumer:(id<InactiveTabsInfoConsumer>)consumer
                    webStateList:(WebStateList*)webStateList
                     prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_MEDIATOR_H_
