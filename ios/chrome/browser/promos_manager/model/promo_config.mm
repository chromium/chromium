// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promo_config.h"

#import "ios/chrome/browser/promos_manager/model/promo_display_context.h"

PromoConfig::PromoConfig(promos_manager::Promo identifier,
                         const base::Feature& feature_engagement_feature,
                         std::optional<PromoDisplayTime> display_time)
    : identifier(identifier),
      feature_engagement_feature(feature_engagement_feature),
      display_time(display_time) {}
