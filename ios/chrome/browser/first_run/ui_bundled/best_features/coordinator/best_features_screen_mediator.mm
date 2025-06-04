// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/shopping_service.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_screen_consumer.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

@implementation BestFeaturesScreenMediator {
  // Segmentation platform service used to retrieve the user's shopper status.
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  // Shopping service used to retrieve the user's Price Tracking eligibility.
  raw_ptr<commerce::ShoppingService> _shoppingService;
  // Whether the user has been classified as a shopping user.
  BOOL _shoppingUser;
}

- (instancetype)
    initWithSegmentationService:
        (segmentation_platform::SegmentationPlatformService*)segmentationService
                shoppingService:(commerce::ShoppingService*)shoppingService {
  self = [super init];
  if (self) {
    _segmentationService = segmentationService;
    _shoppingService = shoppingService;
  }
  return self;
}

- (void)disconnect {
  _segmentationService = nullptr;
  _shoppingService = nullptr;
}

- (void)retrieveShoppingUserSegmentWithCompletion:(ProceduralBlock)completion {
  CHECK(_segmentationService);
  segmentation_platform::PredictionOptions options =
      segmentation_platform::PredictionOptions::ForCached();

  __weak __typeof(self) weakSelf = self;
  auto classificationResultCallback = base::BindOnce(
      [](__typeof(self) strongSelf, ProceduralBlock completion,
         const segmentation_platform::ClassificationResult& shopper_result) {
        [strongSelf didReceiveShopperSegmentationResult:shopper_result];
        if (completion) {
          completion();
        }
      },
      weakSelf, completion);

  _segmentationService->GetClassificationResult(
      segmentation_platform::kShoppingUserSegmentationKey, options, nullptr,
      std::move(classificationResultCallback));
}

#pragma mark - Setters

- (void)setConsumer:(id<BestFeaturesScreenConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setBestFeaturesItems:[self bestFeatureItems]];
}

#pragma mark - Private

// Returns the list of BestFeaturesItems to be shown, based off the status of
// the feature flag and the user's eligibility.
- (NSArray<BestFeaturesItem*>*)bestFeatureItems {
  using enum first_run::BestFeaturesScreenVariationType;
  using enum BestFeaturesItemType;

  std::vector<BestFeaturesItemType> itemTypes;
  first_run::BestFeaturesScreenVariationType variation =
      first_run::GetBestFeaturesScreenVariationType();
  switch (variation) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenBeforeDBPromo:
      itemTypes = {kLensSearch, kEnhancedSafeBrowsing, kLockedIncognitoTabs};
      break;
    case kGeneralScreenWithPasswordItemAfterDBPromo:
      itemTypes = {kLensSearch, kEnhancedSafeBrowsing,
                   kSaveAndAutofillPasswords};
      break;
    case kShoppingUsersWithFallbackAfterDBPromo:
      if (_shoppingUser && _shoppingService->IsShoppingListEligible()) {
        itemTypes = {kTabGroups, kLockedIncognitoTabs,
                     kPriceTrackingAndInsights};
      } else {
        // If the user isn't a shopping user or Price Tracking is not available
        // for them, fallback to other items.
        itemTypes = {kLensSearch, kEnhancedSafeBrowsing,
                     kSaveAndAutofillPasswords};
      }
      break;
    case kSignedInUsersOnlyAfterDBPromo:
      if (password_manager_util::IsCredentialProviderEnabledOnStartup(
              GetApplicationContext()->GetLocalState())) {
        itemTypes = {kLensSearch, kEnhancedSafeBrowsing,
                     kSharePasswordsWithFamily};
      } else {
        itemTypes = {kLensSearch, kEnhancedSafeBrowsing,
                     kAutofillPasswordsInOtherApps};
      }
      break;
    case kDisabled:
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
      NOTREACHED();
  }

  NSMutableArray<BestFeaturesItem*>* items = [NSMutableArray array];
  for (BestFeaturesItemType type : itemTypes) {
    [items addObject:[[BestFeaturesItem alloc] initWithType:type]];
  }
  return items;
}

// Sets the user's shopper segmentation result.
- (void)didReceiveShopperSegmentationResult:
    (const segmentation_platform::ClassificationResult&)shopper_result {
  if (experimental_flags::GetSegmentForForcedShopperExperience() ==
      segmentation_platform::kShoppingUserUmaName) {
    _shoppingUser = YES;
    return;
  }

  if (shopper_result.status ==
      segmentation_platform::PredictionStatus::kSucceeded) {
    // A shopper segment classification result is binary, `ordered_labels`
    // should only have one label.
    if (std::find(shopper_result.ordered_labels.begin(),
                  shopper_result.ordered_labels.end(),
                  segmentation_platform::kShoppingUserUmaName) !=
        shopper_result.ordered_labels.end()) {
      _shoppingUser = YES;
      return;
    }
  }
  _shoppingUser = NO;
}

@end
