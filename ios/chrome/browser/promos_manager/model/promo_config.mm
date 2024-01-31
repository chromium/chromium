// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promo_config.h"

PromoConfig::PromoConfig(promos_manager::Promo identifier,
                         const base::Feature* feature_engagement_feature,
                         NSArray<ImpressionLimit*>* impression_limits)
    : identifier(identifier),
      feature_engagement_feature(feature_engagement_feature),
      impression_limits(impression_limits) {}
