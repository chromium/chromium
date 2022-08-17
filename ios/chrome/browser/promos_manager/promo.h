// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMO_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMO_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/impression_limit.h"

// A promo serves as a uniquely identifiable collection of data describing promo
// display behavior via impression limits.
@interface Promo : NSObject

@property(nonatomic, readonly) NSArray<ImpressionLimit*>* impressionLimits;

// Designated initializer. Initializes with impression limits.
- (instancetype)initWithImpressionLimits:
    (NSArray<ImpressionLimit*>*)impressionLimits NS_DESIGNATED_INITIALIZER;

// Convenience initializer identical to calling:
// [Promo alloc] initWithImpressionLimits:nil];
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMO_H_
