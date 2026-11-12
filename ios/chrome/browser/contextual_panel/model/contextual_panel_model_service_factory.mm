// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"

#import <utility>

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model_factory.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

ContextualPanelModelServiceFactory::ContextualPanelModelServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ContextualPanelModelService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(SamplePanelModelFactory::GetInstance());
  DependsOn(ReaderModeModelFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ContextualPanelModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;

  auto* sample_panel_model_factory =
      SamplePanelModelFactory::GetForProfile(profile);
  if (sample_panel_model_factory &&
      IsContextualPanelForceShowEntrypointEnabled()) {
    models.emplace(ContextualPanelItemType::SamplePanelItem,
                   sample_panel_model_factory);
  }

  // `reader_mode_model` is `nullptr` in Incognito profiles.
  ReaderModeModel* reader_mode_model =
      ReaderModeModelFactory::GetForProfile(profile);
  if (reader_mode_model) {
    models.emplace(ContextualPanelItemType::ReaderModeItem, reader_mode_model);
  }

  return std::make_unique<ContextualPanelModelService>(models);
}
