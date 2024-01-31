// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_IMPRESSION_LIMIT_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_IMPRESSION_LIMIT_H_

#import <Foundation/Foundation.h>

@interface ImpressionLimit : NSObject

// The max number of impressions allowed within `numDays`.
@property(nonatomic, readonly) NSInteger numImpressions;

// The number of days the limit `numImpressions` is restricted to.
@property(nonatomic, readonly) NSInteger numDays;

// Designated initializer. Initializes with an impression count limit for a
// given number of days.
- (instancetype)initWithLimit:(NSInteger)numImpressions
                   forNumDays:(NSInteger)numDays NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_IMPRESSION_LIMIT_H_
