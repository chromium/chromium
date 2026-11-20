// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_mediator.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_screen_consumer.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

@implementation BestFeaturesScreenMediator

- (void)disconnect {
  _consumer = nil;
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
    case kBestOfApp:
      itemTypes = {kLensSearch, kEnhancedSafeBrowsing, kIncognitoBrowsing};
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

@end
