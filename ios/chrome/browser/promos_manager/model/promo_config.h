// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "ios/chrome/browser/promos_manager/model/constants.h"

namespace promos_manager {
enum class Promo;
}

enum class PromoDisplayTime;

// Structure holding the parameters for a promo
struct PromoConfig {
  explicit PromoConfig(
      promos_manager::Promo identifier,
      const base::Feature& feature_engagement_feature,
      std::optional<PromoDisplayTime> display_time = std::nullopt);

  // The promo identifier.
  promos_manager::Promo identifier;

  // The feature engagement tracker feature for this promo.
  raw_ref<const base::Feature> feature_engagement_feature;

  // If present, the promo will only be displayed at this time.
  std::optional<PromoDisplayTime> display_time;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_CONFIG_H_
