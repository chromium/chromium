// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_

enum class LocationBarBadgeType;
@class LocationBarBadgeConfiguration;

// Consumer for the location bar badge mediator.
@protocol LocationBarBadgeConsumer

// Shows/hides badge.
- (void)setBadge:(LocationBarBadgeType)badge hidden:(BOOL)hidden;

// Update the consumer with a new badge configuration.
- (void)setBadgeConfig:(LocationBarBadgeConfiguration*)config;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
