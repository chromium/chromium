// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>

// List of Default Browser Promo screens.
typedef NS_ENUM(NSUInteger, DefaultBrowserScreenConsumerScreenIntent) {
  // Show promo without any disclaimer.
  kDefault,
  // Show promo with terms of service and metric
  // reporting.
  kTOSAndUMA,
  // Show promo with terms of service but without metric
  // reporting. UMA can be disabled for enterprise policy reasons.
  kTOSWithoutUMA,
};

// Consumer protocol for an object that will create a view with a title and
// subtitle.
@protocol DefaultBrowserScreenConsumer <NSObject>

@required
// Shows details (an icon and a footer) that Chrome is managed by platform
// policies. This property needs to be set before the view is loaded.
@property(nonatomic, assign) BOOL hasPlatformPolicies;
// Intent for the screen. This property needs to be set before the view is
// loaded.
@property(nonatomic, assign)
    DefaultBrowserScreenConsumerScreenIntent screenIntent;

// Sets the title text of this view.
- (void)setPromoTitle:(NSString*)titleText;

// Sets the subtitle text of this view.
- (void)setPromoSubtitle:(NSString*)subtitleText;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_CONSUMER_H_
