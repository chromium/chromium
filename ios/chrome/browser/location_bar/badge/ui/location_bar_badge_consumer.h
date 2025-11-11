// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_

@class LocationBarBadgeConfiguration;

// TODO(crbug.com/454351425): Refactor function names to not use "entrypoint".
// Usage is for parity with ContextualPanelEntryPointConsumer.
// Consumer for the location bar badge mediator.
@protocol LocationBarBadgeConsumer

// Update the consumer with a new badge configuration.
- (void)setBadgeConfig:(LocationBarBadgeConfiguration*)config;

// Notify the consumer to show the badge.
- (void)showBadge;

// Notify the consumer to hide badge.
- (void)hideBadge;

// Notify the consumer to collapse badge container. Can correlate to
// transforming the chip into a badge.
- (void)collapseBadgeContainer;

// Notify the consumer to expand badge container. Can correlate to transforming
// the badge into a chip.
- (void)expandBadgeContainer;

// Notify the consumer to highlight the badge. When `highlight` is YES, the
// badge animates to blue, otherwise it animates back to its default color.
- (void)highlightBadge:(BOOL)highlight;

// Checks if the badge is visible.
- (BOOL)isBadgeVisible;

// Shows a blue dot on the badge to indicate being unread.
- (void)showUnreadBadge:(BOOL)unread;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
