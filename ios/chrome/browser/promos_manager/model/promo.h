// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"

// A promo serves as a uniquely identifiable collection of data describing promo
// display behavior via impression limits.
@interface Promo : NSObject

@property(nonatomic, readonly) promos_manager::Promo identifier;
@property(nonatomic, readonly) NSArray<ImpressionLimit*>* impressionLimits;

// Designated initializer. Initializes with a unique identifier
// (promos_manager::Promo) and impression limit(s).
- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier
               andImpressionLimits:(NSArray<ImpressionLimit*>*)impressionLimits
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer for creating a promo without promo-specific
// impression limits; identical to calling:
// [[Promo alloc] initWithIdentifier:`identifier` andImpressionLimits:nil];
- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_H_
