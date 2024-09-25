// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "ios/chrome/browser/promos_manager/model/constants.h"

@class ImpressionLimit;

namespace promos_manager {
enum class Promo;
}

// Structure holding the parameters for a promo
struct PromoConfig {
  explicit PromoConfig(
      promos_manager::Promo identifier,
      const base::Feature* feature_engagement_feature = nullptr,
      NSArray<ImpressionLimit*>* impression_limits = nil);

  // The promo identifier.
  promos_manager::Promo identifier;

  // The feature engagement tracker feature for this promo. May be null if
  // this promo doesn't have a corresponding feature.
  raw_ptr<const base::Feature> feature_engagement_feature;

  // The custom impression limits for this promo. May be null if this feature
  // has no custom limits.
  __strong NSArray<ImpressionLimit*>* impression_limits;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_
