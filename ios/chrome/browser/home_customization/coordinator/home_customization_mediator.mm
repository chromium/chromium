// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/utils.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_metrics_recorder.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "url/gurl.h"

@implementation HomeCustomizationMediator {
  // Pref service to handle preference changes.
  raw_ptr<PrefService, DanglingUntriaged> _prefService;
  // Browser agent to be notified of Discover eligibility.
  raw_ptr<DiscoverFeedVisibilityBrowserAgent, DanglingUntriaged>
      _discoverFeedVisibilityBrowserAgent;
  // ShoppingService used to determine ShopCard toggle
  // eligibility.
  raw_ptr<commerce::ShoppingService> _shoppingService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
                       shoppingService:
                           (commerce::ShoppingService*)shoppingService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _discoverFeedVisibilityBrowserAgent = discoverFeedVisibilityBrowserAgent;
    _shoppingService = shoppingService;
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
  };
  if (_discoverFeedVisibilityBrowserAgent->GetEligibility() ==
      DiscoverFeedEligibility::kEligible) {
    toggleMap.insert(
        {CustomizationToggleType::kDiscover,
         [self isModuleEnabledForType:CustomizationToggleType::kDiscover]});
  }
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
  std::map<CustomizationToggleType, BOOL> toggleMap = {
      {CustomizationToggleType::kSetUpList,
       [self
           isMagicStackCardEnabledForType:CustomizationToggleType::kSetUpList]},
      {CustomizationToggleType::kSafetyCheck,
       [self isMagicStackCardEnabledForType:CustomizationToggleType::
                                                kSafetyCheck]},
      {CustomizationToggleType::kTapResumption,
       [self isMagicStackCardEnabledForType:CustomizationToggleType::
                                                kTapResumption]},
  };
  if (IsTipsMagicStackEnabled()) {
    toggleMap.insert(
        {CustomizationToggleType::kTips,
         [self isMagicStackCardEnabledForType:CustomizationToggleType::kTips]});
  }
  if (_shoppingService && _shoppingService->IsShoppingListEligible()) {
    toggleMap.insert({CustomizationToggleType::kShopCard,
                      [self isMagicStackCardEnabledForType:
                                CustomizationToggleType::kShopCard]});
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
      return _discoverFeedVisibilityBrowserAgent->IsEnabled();
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
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTipsEnabled);
    }
    case CustomizationToggleType::kShopCard:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled);
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
      _discoverFeedVisibilityBrowserAgent->SetEnabled(enabled);
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
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled,
                               enabled);
      break;
    }
    case CustomizationToggleType::kShopCard:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
          enabled);
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
      break;
    case CustomizationLinkType::kEnterpriseLearnMore:
      URL = GURL(kManagementLearnMoreURL);
      break;
  }
  [self.navigationDelegate navigateToURL:URL];
}

- (void)dismissMenuPage {
  [self.navigationDelegate dismissMenuPage];
}

@end
