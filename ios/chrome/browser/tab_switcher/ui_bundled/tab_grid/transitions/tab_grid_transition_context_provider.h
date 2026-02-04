// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_CONTEXT_PROVIDER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_CONTEXT_PROVIDER_H_

#import <UIKit/UIKit.h>

@class NamedGuide;

// Protocol providing context for the TabGrid transition.
@protocol TabGridTransitionContextProvider <NSObject>

// Returns the content area guide used for the transition.
- (NamedGuide*)contentAreaGuide;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_CONTEXT_PROVIDER_H_
