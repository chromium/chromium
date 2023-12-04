// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, NotificationsExperimentType) {
  NotificationsExperimentTypeEnabled = 0,
  NotificationsExperimentTypePromoEnabled = 1,
  NotificationsExperimentTypeSetUpListsEnabled = 2,
};

extern NSString* const kNotificationsPromoCloseButtonId;
extern NSString* const kNotificationsPromoPrimaryButtonId;

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
