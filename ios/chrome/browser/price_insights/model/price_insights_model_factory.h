// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_

#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PriceInsightsModels and associates them with
// profile.
class PriceInsightsModelFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PriceInsightsModelFactory,
                                                PriceInsightsModel> {
 public:
  explicit PriceInsightsModelFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_
