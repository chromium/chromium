// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/constants.h"

#import <Foundation/Foundation.h>

#import "base/notreached.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace promos_manager {

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
Promo PromoForName(std::string promo) {
  if (promo == "promos_manager::Promo::Test") {
    return promos_manager::Promo::Test;
  } else {
    NOTREACHED();

    // Returns promos_manager::Promo::Test by default, but this should never be
    // reached!
    return promos_manager::Promo::Test;
  }
}

std::string NameForPromo(Promo promo) {
  switch (promo) {
    case promos_manager::Promo::Test:
      return "promos_manager::Promo::Test";
  }
}

NSArray<ImpressionLimit*>* GlobalImpressionLimits() {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    ImpressionLimit* onceEveryTwoDays =
        [[ImpressionLimit alloc] initWithLimit:1 forNumDays:2];
    ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                                 forNumDays:7];
    limits = @[ onceEveryTwoDays, thricePerWeek ];
  });

  return limits;
}

NSArray<ImpressionLimit*>* GlobalPerPromoImpressionLimits() {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    ImpressionLimit* oncePerMonth = [[ImpressionLimit alloc] initWithLimit:1
                                                                forNumDays:30];
    limits = @[ oncePerMonth ];
  });

  return limits;
}

}  // namespace promos_manager
