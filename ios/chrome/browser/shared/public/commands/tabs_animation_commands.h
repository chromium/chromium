// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_

#include <objc/NSObjCRuntime.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

// Protocol for animating tabs in the tab grid.
@protocol TabsAnimationCommands

// Triggers the tabs closure animation on the tab grid for the WebStates in
// `tabsToClose`, for the groups in `groupsWithTabsToClose`, and if
// `animateAllInactiveTabs` is true, then for the inactive tabs banner. It also
// triggers the closure of the selected WebStates through`completionHandler`
// after running the animation.
- (void)animateTabsClosureForTabs:(std::set<web::WebStateID>)tabsToClose
                           groups:
                               (std::map<tab_groups::TabGroupId, std::set<int>>)
                                   groupsWithTabsToClose
                  allInactiveTabs:(BOOL)animateAllInactiveTabs
                completionHandler:(ProceduralBlock)completionHandler;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_
