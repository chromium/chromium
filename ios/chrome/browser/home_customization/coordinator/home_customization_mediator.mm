// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/i18n/number_formatting.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/utils.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_metrics_recorder.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

@implementation HomeCustomizationMediator {
  // Pref service to handle preference changes.
  raw_ptr<PrefService> _prefService;
  // Browser agent to be notified of Discover eligibility.
  raw_ptr<DiscoverFeedVisibilityBrowserAgent>
      _discoverFeedVisibilityBrowserAgent;
  // The image fetcher used to download individual background images.
  raw_ptr<image_fetcher::ImageFetcher> _imageFetcher;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
                   imageFetcherService:(image_fetcher::ImageFetcherService*)
                                           imageFetcherService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _discoverFeedVisibilityBrowserAgent = discoverFeedVisibilityBrowserAgent;
    _imageFetcher = imageFetcherService->GetImageFetcher(
        image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
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

  if (IsNTPBackgroundCustomizationEnabled()) {
    NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*
        backgroundCustomizationConfigurationMap =
            [NSMutableDictionary dictionary];

    // TODO(crbug.com/408243803): fetch background customization
    // configurations and fill the `backgroundCustomizationConfigurationMap` and
    // `selectedBackgroundId`.
    [self.mainPageConsumer populateBackgroundCustomizationConfigurations:
                               backgroundCustomizationConfigurationMap
                                                    selectedBackgroundId:nil];
  }
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
  if (IsIOSParcelTrackingEnabled()) {
    toggleMap.insert({CustomizationToggleType::kParcelTracking,
                      [self isMagicStackCardEnabledForType:
                                CustomizationToggleType::kParcelTracking]});
  }
  if (IsTipsMagicStackEnabled()) {
    toggleMap.insert(
        {CustomizationToggleType::kTips,
         [self isMagicStackCardEnabledForType:CustomizationToggleType::kTips]});
  }
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1 ||
      commerce::kShopCardVariation.Get() == commerce::kShopCardArm2) {
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
    case CustomizationToggleType::kParcelTracking:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackParcelTrackingEnabled);
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTipsEnabled);
    }
    case CustomizationToggleType::kShopCard:
      if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
        return _prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled);
      } else if (commerce::kShopCardVariation.Get() ==
                 commerce::kShopCardArm2) {
        return _prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled);
      } else {
        return false;
      }
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
    case CustomizationToggleType::kParcelTracking:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackParcelTrackingEnabled, enabled);
      break;
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled,
                               enabled);
      break;
    }
    case CustomizationToggleType::kShopCard:
      if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
        _prefService->SetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
            enabled);
      } else if (commerce::kShopCardVariation.Get() ==
                 commerce::kShopCardArm2) {
        _prefService->SetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled, enabled);
      }
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

- (void)applyBackgroundForConfiguration:
    (BackgroundCustomizationConfiguration*)backgroundConfiguration {
  // TODO(crbug.com/408243803): apply NTP background configuration to NTP.
}

- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:
                                               (void (^)(UIImage*))completion {
  CHECK(!thumbnailURL.is_empty());
  CHECK(thumbnailURL.is_valid());

  _imageFetcher->FetchImage(
      thumbnailURL,
      base::BindOnce(^(const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
        if (!image.IsEmpty()) {
          UIImage* uiImage = image.ToUIImage();
          if (completion) {
            completion(uiImage);
          }
        }
      }),
      // TODO (crbug.com/417234848): Add annotation.
      image_fetcher::ImageFetcherParams(NO_TRAFFIC_ANNOTATION_YET, "Test"));
}

@end
