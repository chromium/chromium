// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"

@protocol NewTabPageDelegate;

// The top-level owner of the feed top section.
@interface FeedTopSectionCoordinator
    : ChromeCoordinator <NotificationsAlertPresenter>

@property(nonatomic, readonly, strong) UIViewController* viewController;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// Handles the feed top section's signin promo changing visibility.
- (void)signinPromoHasChangedVisibility:(BOOL)visible;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_COORDINATOR_H_
