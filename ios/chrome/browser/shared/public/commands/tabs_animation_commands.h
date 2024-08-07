// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

// Protocol for animating tabs in the tab grid.
@protocol TabsAnimationCommands

// Triggers the tabs closure animation on the tab grid for the WebStates in
// `tabsToClose. It also closes the WebStates after running the animation.
- (void)animateTabsClosureForTabs:(std::set<web::WebStateID>)tabsToClose;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TABS_ANIMATION_COMMANDS_H_
