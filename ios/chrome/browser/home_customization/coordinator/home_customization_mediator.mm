// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_metrics_recorder.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "url/gurl.h"

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

- (void)configureDiscoverPageData {
  std::vector<CustomizationLinkType> linksVector = {
      CustomizationLinkType::kFollowing,
      CustomizationLinkType::kHidden,
      CustomizationLinkType::kActivity,
      CustomizationLinkType::kLearnMore,
  };
  [self.discoverPageConsumer populateDiscoverLinks:linksVector];
}

- (void)configureMagicStackPageData {
  std::map<CustomizationToggleType, BOOL> toggleMap = {};
  toggleMap.insert({CustomizationToggleType::kSetUpList,
                    [self isMagicStackCardEnabledForType:
                              CustomizationToggleType::kSetUpList]});
  toggleMap.insert({CustomizationToggleType::kSafetyCheck,
                    [self isMagicStackCardEnabledForType:
                              CustomizationToggleType::kSafetyCheck]});
  toggleMap.insert({CustomizationToggleType::kTapResumption,
                    [self isMagicStackCardEnabledForType:
                              CustomizationToggleType::kTapResumption]});
  if (IsIOSParcelTrackingEnabled()) {
    toggleMap.insert({CustomizationToggleType::kParcelTracking,
                      [self isMagicStackCardEnabledForType:
                                CustomizationToggleType::kParcelTracking]});
  }
  [self.magicStackPageConsumer populateToggles:toggleMap];
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
      return _prefService->GetBoolean(prefs::kArticlesForYouEnabled);
    default:
      NOTREACHED();
  }
}

// Returns whether the Magic Stack card with `type` is enabled in the
// preferences.
- (BOOL)isMagicStackCardEnabledForType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kSetUpList:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled);
    case CustomizationToggleType::kSafetyCheck:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSafetyCheckEnabled);
    case CustomizationToggleType::kTapResumption:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTabResumptionEnabled);
    case CustomizationToggleType::kParcelTracking:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackParcelTrackingEnabled);
    default:
      NOTREACHED();
  }
}

#pragma mark - HomeCustomizationMutator

- (void)toggleModuleVisibilityForType:(CustomizationToggleType)type
                              enabled:(BOOL)enabled {
  [HomeCustomizationMetricsRecorder recordCellToggled:type];
  switch (type) {
    // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      _prefService->SetBoolean(prefs::kHomeCustomizationMostVisitedEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kMagicStack:
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kDiscover:
      _prefService->SetBoolean(prefs::kArticlesForYouEnabled, enabled);
      break;

    // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled, enabled);
      break;
    case CustomizationToggleType::kSafetyCheck:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackSafetyCheckEnabled, enabled);
      break;
    case CustomizationToggleType::kTapResumption:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackTabResumptionEnabled, enabled);
      break;
    case CustomizationToggleType::kParcelTracking:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackParcelTrackingEnabled, enabled);
      break;
  }
}

- (void)navigateToSubmenuForType:(CustomizationToggleType)type {
  [self.navigationDelegate
      presentCustomizationMenuPage:[HomeCustomizationHelper
                                       menuPageForToggleType:type]];
}

- (void)navigateToLinkForType:(CustomizationLinkType)type {
  GURL URL;
  switch (type) {
    case CustomizationLinkType::kFollowing:
      URL = GURL(kDiscoverFollowingURL);
      break;
    case CustomizationLinkType::kHidden:
      URL = GURL(kDiscoverHiddenURL);
      break;
    case CustomizationLinkType::kActivity:
      URL = GURL(kDiscoverActivityURL);
      break;
    case CustomizationLinkType::kLearnMore:
      URL = GURL(kDiscoverLearnMoreURL);
  }
  [self.navigationDelegate navigateToURL:URL];
}

- (void)dismissMenuPage {
  [self.navigationDelegate dismissMenuPage];
}

@end
