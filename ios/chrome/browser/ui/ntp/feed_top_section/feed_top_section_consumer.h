// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_

@class SigninPromoViewConfigurator;

// Protocol used to communicate with the Feed Top Section View.
@protocol FeedTopSectionConsumer

// Indicates if the consumer should hide or show the signin-promo.
@property(nonatomic, assign) BOOL shouldShowSigninPromo;

// Allows the consumer to use the `configurator` to configure its view.
- (void)updateSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)configurator;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_CONSUMER_H_
