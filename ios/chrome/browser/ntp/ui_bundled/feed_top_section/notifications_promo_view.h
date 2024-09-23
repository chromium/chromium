// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"

@protocol FeedTopSectionMutator;

@interface NotificationsPromoView : UIView

@property(nonatomic, weak) id<FeedTopSectionMutator> mutator;

// Designated initializer.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_NOTIFICATIONS_PROMO_VIEW_H_
