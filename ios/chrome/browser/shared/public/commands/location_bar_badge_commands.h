// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LOCATION_BAR_BADGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LOCATION_BAR_BADGE_COMMANDS_H_

#import <UIKit/UIKit.h>

@class LocationBarBadgeConfiguration;

// Protocol for location bar badge commands.
@protocol LocationBarBadgeCommands

// Updates badge configuration for the location bar badge.
- (void)updateBadgeConfig:(LocationBarBadgeConfiguration*)config;

// Updates badge with IPH related colors.
- (void)updateColorForIPH;

// Whether to display a blue dot indicating an unread badge.
- (void)markDisplayedBadgeAsUnread:(BOOL)read;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LOCATION_BAR_BADGE_COMMANDS_H_
