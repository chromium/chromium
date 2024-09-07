// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"

NSString* const kNotificationsPromoCloseButtonId =
    @"NotificationsPromoCloseButtonId";
NSString* const kNotificationsPromoPrimaryButtonId =
    @"NotificationsPromoPrimaryButtonId";
NSString* const kNotificationsPromoSecondaryButtonId =
    @"NotificationsPromoSecondaryButtonId";

int const kNotificationsPromoMaxDismissedCount = 2;
int const kNotificationsPromoMaxShownCount = 6;
base::TimeDelta const kNotificationsPromoDismissedCooldownTime = base::Days(14);
base::TimeDelta const kNotificationsPromoShownCooldownTime = base::Minutes(30);

int const kMaxImpressionsForDismissedThreshold = 9999;
