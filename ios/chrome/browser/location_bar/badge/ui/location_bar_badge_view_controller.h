// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"

@class LocationBarBadgesContainerView;

// View controller for the location bar badge.
// TODO(crbug.com/445719031): Implement this.
@interface LocationBarBadgeViewController
    : UIViewController <LocationBarBadgeConsumer>

@property(nonatomic, readonly)
    LocationBarBadgesContainerView* badgesContainerView;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_
