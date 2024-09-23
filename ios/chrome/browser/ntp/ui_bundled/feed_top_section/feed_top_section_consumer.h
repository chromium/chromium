// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_

@class SigninPromoViewConfigurator;

typedef NS_ENUM(NSInteger, PromoViewType) {
  // Standard style used for the Content Notifications.
  PromoViewTypeSignin = 0,
  PromoViewTypeNotifications,
};

// Protocol used to communicate with the Feed Top Section View.
@protocol FeedTopSectionConsumer

// Property that stores the PromoViewType that is currently being displayed.
@property(nonatomic, assign) PromoViewType visiblePromoViewType;

// Allows the consumer to use the `configurator` to configure its view.
- (void)updateSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)configurator;

// Methods to show/hide the promo.
- (void)showPromo;
- (void)hidePromo;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_
