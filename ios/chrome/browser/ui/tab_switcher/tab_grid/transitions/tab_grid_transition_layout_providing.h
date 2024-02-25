// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_PROVIDING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_PROVIDING_H_

#import <Foundation/Foundation.h>

@class TabGridTransitionLayout;

// Objects conforming to this protocol can provide information for the
// animation of the transitions from and to the tab grid.
@protocol TabGridTransitionLayoutProviding

// Asks the provider for the layout of the tab grid to be used in transition
// animations.
- (TabGridTransitionLayout*)transitionLayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_LAYOUT_PROVIDING_H_
