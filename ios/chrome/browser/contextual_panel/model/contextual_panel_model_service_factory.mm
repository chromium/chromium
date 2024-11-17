// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model_factory.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

// static
ContextualPanelModelService* ContextualPanelModelServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ContextualPanelModelService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
ContextualPanelModelServiceFactory*
ContextualPanelModelServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualPanelModelServiceFactory> instance;
  return instance.get();
}

ContextualPanelModelServiceFactory::ContextualPanelModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContextualPanelModelService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SamplePanelModelFactory::GetInstance());
  DependsOn(PriceInsightsModelFactory::GetInstance());
}

ContextualPanelModelServiceFactory::~ContextualPanelModelServiceFactory() {}

std::unique_ptr<KeyedService>
ContextualPanelModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
  if (IsContextualPanelForceShowEntrypointEnabled()) {
    models.emplace(ContextualPanelItemType::SamplePanelItem,
                   SamplePanelModelFactory::GetForProfile(profile));
  }

  if (IsPriceInsightsEnabled(profile)) {
    models.emplace(ContextualPanelItemType::PriceInsightsItem,
                   PriceInsightsModelFactory::GetForProfile(profile));
  }
  return std::make_unique<ContextualPanelModelService>(models);
}
