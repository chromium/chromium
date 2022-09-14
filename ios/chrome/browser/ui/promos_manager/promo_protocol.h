// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMO_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMO_PROTOCOL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"

// PromoProtocol defines the minimum set of data required to create a
// Promo maintained by the PromosManager. A promo object must have a
// unique mapping to a promo enum (promos_manager::Promo), and
// (optionally) can define promo-specific impression limits to control
// its display behavior.
@protocol PromoProtocol <NSObject>

@optional

// The promo-specific impression limits.
- (NSArray<ImpressionLimit*>*)impressionLimits;

@required

// Which promos_manager::Promo the object is uniquely associated with.
- (promos_manager::Promo)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMO_PROTOCOL_H_
