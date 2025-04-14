// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_H_
#define IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_H_

#import <UIKit/UIKit.h>

// A UIButton that enables the user to begin tracking the item's price.
@interface PriceNotificationsTrackButton : UIButton
// Show the lighter variant used by ShopCard
- (instancetype)initWithLightVariant:(BOOL)useLightVariant
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Whether to show the lighter variant used by ShopCard
@property(nonatomic, assign) BOOL useLightVariant;
@end

#endif  // IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_H_
