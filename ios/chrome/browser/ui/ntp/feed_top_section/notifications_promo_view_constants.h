// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import "base/time/time.h"

typedef NS_ENUM(NSInteger, NotificationsExperimentType) {
  NotificationsExperimentTypeEnabled = 0,
  NotificationsExperimentTypePromoEnabled = 1,
  NotificationsExperimentTypeSetUpListsEnabled = 2,
  NotificationsExperimentTypeProvisional = 3,
};

typedef NS_ENUM(NSInteger, NotificationsPromoButtonType) {
  NotificationsPromoButtonTypePrimary = 0,
  NotificationsPromoButtonTypeSecondary = 1,
  NotificationsPromoButtonTypeClose = 2,
};

extern NSString* const kNotificationsPromoCloseButtonId;
extern NSString* const kNotificationsPromoPrimaryButtonId;
extern NSString* const kNotificationsPromoSecondaryButtonId;

extern int const kNotificationsPromoMaxDismissedCount;
extern int const kNotificationsPromoMaxShownCount;
extern base::TimeDelta const kNotificationsPromoDismissedCooldownTime;
extern base::TimeDelta const kNotificationsPromoShownCooldownTime;

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
