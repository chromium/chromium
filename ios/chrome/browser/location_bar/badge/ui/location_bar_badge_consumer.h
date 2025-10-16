// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_

#import "ios/chrome/browser/location_bar/badge/ui/badge_type.h"

// Consumer for the location bar badge mediator.
@protocol LocationBarBadgeConsumer

// Shows/hides `feature` badge.
- (void)setFeature:(BadgeType)feature hidden:(BOOL)hidden;
@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_CONSUMER_H_
