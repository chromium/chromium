// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/model/price_insights_model_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
PriceInsightsModel* PriceInsightsModelFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<PriceInsightsModel*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
PriceInsightsModelFactory* PriceInsightsModelFactory::GetInstance() {
  static base::NoDestructor<PriceInsightsModelFactory> instance;
  return instance.get();
}

PriceInsightsModelFactory::PriceInsightsModelFactory()
    : BrowserStateKeyedServiceFactory(
          "PriceInsightsModel",
          BrowserStateDependencyManager::GetInstance()) {}

PriceInsightsModelFactory::~PriceInsightsModelFactory() {}

std::unique_ptr<KeyedService>
PriceInsightsModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<PriceInsightsModel>();
}
