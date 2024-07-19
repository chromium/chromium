// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation HomeCustomizationMediator {
  // Pref service to handle preference changes.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - Public

- (void)configureMainPageData {
  std::map<CustomizationToggleType, BOOL> toggleMap = {
      {CustomizationToggleType::kMostVisited,
       [self isModuleEnabledForType:CustomizationToggleType::kMostVisited]},
      {CustomizationToggleType::kMagicStack,
       [self isModuleEnabledForType:CustomizationToggleType::kMagicStack]},
      {CustomizationToggleType::kDiscover,
       [self isModuleEnabledForType:CustomizationToggleType::kDiscover]},
  };
  [self.mainPageConsumer populateToggles:toggleMap];
}

#pragma mark - Private

// Returns whether the module with `type` is enabled in the preferences.
- (BOOL)isModuleEnabledForType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMostVisitedEnabled);
    case CustomizationToggleType::kMagicStack:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackEnabled);
    case CustomizationToggleType::kDiscover:
      return _prefService->GetBoolean(prefs::kHomeCustomizationDiscoverEnabled);
  }
}

#pragma mark - HomeCustomizationMutator

- (void)handleModuleToggledWithType:(CustomizationToggleType)type
                            enabled:(BOOL)enabled {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      _prefService->SetBoolean(prefs::kHomeCustomizationMostVisitedEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kMagicStack:
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kDiscover:
      _prefService->SetBoolean(prefs::kHomeCustomizationDiscoverEnabled,
                               enabled);
      break;
  }
}

@end
