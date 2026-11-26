// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_MUTATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_MUTATOR_H_

#import <CoreGraphics/CoreGraphics.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_button_type.h"

// Handles the actions of the toolbar's control buttons.
@protocol ToolbarButtonsMutator

// Navigates back.
- (void)goBack;

// Navigates forward.
- (void)goForward;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stop;

// Called when the assistant button is tapped.
- (void)assistantButtonTapped;

// Opens a new tab, incognito in an incognito browser, animated from
// `originPoint`, in window coordinates.
- (void)openNewTabFromPoint:(CGPoint)originPoint;

// Records the user actions of a tap on the button of `type`, which depend on
// the active WebState (e.g. the NTP variants).
- (void)recordUserActionsForButtonTapped:(ToolbarButtonType)type;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTONS_MUTATOR_H_
