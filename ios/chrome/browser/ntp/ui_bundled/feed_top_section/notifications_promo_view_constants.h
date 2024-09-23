// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import "base/time/time.h"

// Enum actions for content notification promo UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationTopOfFeedPromoAction enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationTopOfFeedPromoAction {
  kAccept = 0,
  kDecline = 1,
  kMainButtonTapped = 2,
  kDismissedFromCloseButton = 3,
  kDismissedFromSecondaryButton = 4,
  kDisplayed = 5,
  kMaxValue = kDisplayed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Enum events for content notification promo UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationTopOfFeedPromoEvent enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationTopOfFeedPromoEvent {
  kPromptShown = 0,
  kNotifActive = 1,
  kError = 2,
  kCanceled = 3,
  kMaxValue = kCanceled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

typedef NS_ENUM(NSInteger, NotificationsExperimentType) {
  NotificationsExperimentTypeEnabled = 0,
  NotificationsExperimentTypePromoEnabled = 1,
  NotificationsExperimentTypeSetUpListsEnabled = 2,
  NotificationsExperimentTypeProvisional = 3,
  NotificationsExperimentTypeProvisionalBypassDeprecatedDoNoUse = 4,
  NotificationsExperimentTypePromoRegistrationOnly = 5,
  NotificationsExperimentTypeProvisionalRegistrationOnly = 6,
  NotificationsExperimentTypeSetUpListsRegistrationOnly = 7,
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

extern int const kMaxImpressionsForDismissedThreshold;

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_CONSTANTS_H_
