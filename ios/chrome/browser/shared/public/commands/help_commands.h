// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

#import <Foundation/Foundation.h>

/// Types of in-product help managed by bubble presenter coordinator.
///
/// Note: This is NOT a conclusive list of all help bubbles on Chrome on iOS,
/// since some of them are directly managed by feature coordinators.
enum class InProductHelpType : NSInteger {
  /// Discover feed's menu button.
  kDiscoverFeedMenu,
  /// Home customization menu entrypoint.
  kHomeCustomizationMenu,
  /// Follow help bubble while browsing a site.
  kFollowWhileBrowsing,
  /// Help bubble to let the user know that they can change the default mode
  /// (Desktop/Mobile) of the websites.
  kDefaultSiteView,
  /// Help bubble for What's New.
  kWhatsNew,
  /// Help bubble to inform the user that they can track the price of the item
  /// on the current website.
  kPriceNotificationsWhileBrowsing,
  /// Help bubble to inform the user that they can tap the Lens button in the
  /// omnibox keyboard to search with their camera.
  kLensKeyboard,
  /// Help bubble to inform the user that their tracked packages will appear in
  /// the Magic Stack.
  kParcelTracking,
  /// Fullscreen help bubble for the pull-to-refresh gesture.
  kPullToRefresh,
  /// Fullscreen help bubble for the gesture to swipe to navigate back/forward.
  kBackForwardSwipe,
  /// Fullscreen help bubble for the gesture to swipe horizontally on the
  /// toolbar to switch tabs.
  kToolbarSwipe,
  /// Help bubble for the lens overlay feature entrypoint.
  kLensOverlayEntrypoint
};

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
