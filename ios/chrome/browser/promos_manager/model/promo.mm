// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promo.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/model/constants.h"

@implementation Promo

- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier {
  return [self initWithIdentifier:identifier andImpressionLimits:nil];
}

- (instancetype)initWithIdentifier:(promos_manager::Promo)identifier
               andImpressionLimits:
                   (NSArray<ImpressionLimit*>*)impressionLimits {
  if ((self = [super init])) {
    _identifier = identifier;
    _impressionLimits = impressionLimits;
  }

  return self;
}

@end
