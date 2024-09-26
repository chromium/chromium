// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PriceInsightsModel;

// Singleton that owns all PriceInsightsModels and associates them with
// profile.
class PriceInsightsModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PriceInsightsModel* GetForProfile(ProfileIOS* profile);
  static PriceInsightsModelFactory* GetInstance();

  PriceInsightsModelFactory(const PriceInsightsModelFactory&) = delete;
  PriceInsightsModelFactory& operator=(const PriceInsightsModelFactory&) =
      delete;

 private:
  friend class base::NoDestructor<PriceInsightsModelFactory>;

  PriceInsightsModelFactory();
  ~PriceInsightsModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_MODEL_FACTORY_H_
