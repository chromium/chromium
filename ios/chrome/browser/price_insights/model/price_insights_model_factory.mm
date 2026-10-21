// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model_factory.h"

#import <utility>

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

PriceInsightsModelFactory::PriceInsightsModelFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "PriceInsightsModel") {
}

std::unique_ptr<KeyedService>
PriceInsightsModelFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<PriceInsightsModel>();
}
