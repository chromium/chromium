// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_

enum class LocationBarBadgeType;
@class LocationBarBadgeConfiguration;

// TODO(crbug.com/454351425): Refactor function names to not use "entrypoint".
// Usage is for parity with ContextualPanelEntryPointConsumer.
// Consumer for the location bar badge mediator.
@protocol LocationBarBadgeConsumer

// Shows/hides badge.
- (void)setBadge:(LocationBarBadgeType)badge hidden:(BOOL)hidden;

// Update the consumer with a new badge configuration.
- (void)setBadgeConfig:(LocationBarBadgeConfiguration*)config;

// Notify the consumer to show the entrypoint.
- (void)showEntrypoint;

// Notify the consumer to transition back to the small entrypoint.
- (void)transitionToSmallEntrypoint;

// Notify the consumer to transition to the large entrypoint for a loud moment.
- (void)transitionToLargeEntrypoint;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
