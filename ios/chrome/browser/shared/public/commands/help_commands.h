// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

// Commands to control the display of in-product help UI ("bubbles").
@protocol HelpCommands <NSObject>

// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

// Shows a help bubble for the share button, if eligible.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentShareButtonHelpBubbleIfEligible;

// Optionally presents a bubble associated with the tab grid iph. If the feature
// engagement tracker determines it is valid to show the new tab tip, then it
// initializes `tabGridIPHBubblePresenter` and presents the bubble. If it is
// not valid to show the new tab tip, `tabGridIPHBubblePresenter` is set to
// `nil` and no bubble is shown. This method requires that `self.browserState`
// is not NULL.
- (void)presentTabGridToolbarItemBubble;

// Optionally presents a bubble associated with the new tab iph. If the feature
// engagement tracker determines it is valid to show the new tab tip, then it
// initializes `openNewTabIPHBubblePresenter` and presents the bubble. If it is
// not valid to show the new tab tip, `openNewTabIPHBubblePresenter` is set to
// `nil` and no bubble is shown. This method requires that `self.browserState`
// is not NULL.
- (void)presentNewTabToolbarItemBubble;

// Optionally presents a full screen IPH associated with
// the pull-to-refresh feature. If the feature engagement tracker determines the
// pull-to-refresh tip should be shown, then it initializes
// `pullToRefreshSideSwipeBubble` and presents a SideSwipeBubbleView, otherwise
// it sets `pullToRefreshSideSwipeBubble` to `nil` and no gestural tip is shown.
- (void)presentPullToRefreshSideSwipeBubble;

// Removes the IPH shown by `presentPullToRefreshSideSwipeBubble` if it exists,
// but does nothing otherwise. The presenter of the pull-to-refresh IPH should
// make sure it's called when the user leaves the refreshed website, especially
// while the IPH is still visible.
- (void)removePullToRefreshSideSwipeBubble;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
