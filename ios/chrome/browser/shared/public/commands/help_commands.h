// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

#import "ios/chrome/browser/bubble/public/in_product_help_type.h"

#import <Foundation/Foundation.h>

/// Commands to control the display of in-product help UI ("bubbles").
@protocol HelpCommands <NSObject>

/// Optionally presents an in-product help bubble of `type`. The eligibility
/// can depend on the UI hierarchy at the moment, the configuration and the
/// display history of the bubble, etc.
- (void)presentInProductHelpWithType:(InProductHelpType)type;

/// Delegate method to be invoked when the user has performed a swipe on the
/// toolbar to switch tabs. Remove `toolbarSwipeGestureIPH` if visible.
- (void)handleToolbarSwipeGesture;

/// Delegate method to be invoked when a gestural in-product help view is
/// visible but the user has tapped outside of it. Do nothing if invoked when
/// there is no IPH view.
- (void)handleTapOutsideOfVisibleGestureInProductHelp;

/// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
