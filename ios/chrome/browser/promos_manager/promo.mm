// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promo.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation Promo

- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier {
  return [self initWithIdentifier:identifier andImpressionLimits:nil];
}

- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier
               andImpressionLimits:
                   (NSArray<ImpressionLimit*>*)impressionLimits {
  if (self = [super init]) {
    _identifier = identifier;
    _impressionLimits = impressionLimits;
  }

  return self;
}

@end
