// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_PUBLIC_IN_PRODUCT_HELP_TYPE_H_
#define IOS_CHROME_BROWSER_BUBBLE_PUBLIC_IN_PRODUCT_HELP_TYPE_H_

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
  /// Fullscreen help bubble for the pull-to-refresh gesture.
  kPullToRefresh,
  /// Fullscreen help bubble for the gesture to swipe to navigate back/forward.
  kBackForwardSwipe,
  /// Fullscreen help bubble for the gesture to swipe horizontally on the
  /// toolbar to switch tabs.
  kToolbarSwipe,
  /// Help bubble for the lens overlay feature entrypoint.
  kLensOverlayEntrypoint,
  /// Help bubble to point the user to "Settings" in the overflow menu.
  kSettingsInOverflowMenu,
  /// Help bubble for swiping on the feed.
  kFeedSwipe,
  /// Help bubble for account switching via the account particle disc on the New
  /// Tab page.
  kSwitchAccountsWithNTPAccountParticleDisc,
  /// Help bubble for page action menu.
  kPageActionMenu,
  /// Help bubble for Reader Mode options menu.
  kReaderModeOptions,
};

#endif  // IOS_CHROME_BROWSER_BUBBLE_PUBLIC_IN_PRODUCT_HELP_TYPE_H_
