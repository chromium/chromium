// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MUTATOR_H_

#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view_constants.h"

@protocol FeedTopSectionMutator

// Handles a tap on the Close/secondary button of the notifications promo.
- (void)notificationsPromoViewDismissedFromButton:
    (NotificationsPromoButtonType)buttonType;
// Handles a tap on the main button of the notifications promo.
- (void)notificationsPromoViewMainButtonWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MUTATOR_H_
