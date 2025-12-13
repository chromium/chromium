// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_mediator.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_screen_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation WelcomeBackMediator {
  // Authentication service.
  raw_ptr<AuthenticationService> _authenticationService;
  // Account manager service.
  raw_ptr<ChromeAccountManagerService> _chromeAccountManagerService;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _chromeAccountManagerService = accountManagerService;
  }
  return self;
}

- (void)disconnect {
  _authenticationService = nil;
  _chromeAccountManagerService = nil;
}

#pragma mark - Setters

- (void)setConsumer:(id<WelcomeBackScreenConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setWelcomeBackItems:[self itemsToDisplay]];

  bool isSignedIn =
      _authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  // Feed the correct string and avatar (if applicable) to the consumer.
  if (isSignedIn) {
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    UIImage* avatarImage =
        GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
            identity, IdentityAvatarSize::Large);

    [_consumer setTitle:l10n_util::GetNSStringF(
                            IDS_IOS_WELCOME_BACK_TITLE_SIGNED_IN,
                            base::SysNSStringToUTF16(identity.userGivenName))];
    [_consumer setAvatar:avatarImage];
  } else {
    [_consumer
        setTitle:l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_TITLE_DEFAULT)];
  }
}

#pragma mark - Private

// Set the items to display based on the preferred items for the expirement arm
// and item eligibility. If an item is ineligible replace it with the next
// available eligible item.
- (NSArray<BestFeaturesItem*>*)itemsToDisplay {
  std::vector<BestFeaturesItemType> itemTypesToDisplay;
  std::vector<BestFeaturesItemType> preferredItems = [self preferredItems];
  std::vector<BestFeaturesItemType> eligibleItemsVector =
      GetWelcomeBackEligibleItems();
  std::set<BestFeaturesItemType> eligibleItems(eligibleItemsVector.begin(),
                                               eligibleItemsVector.end());

  // Add the eligible preferred items.
  for (BestFeaturesItemType preferredItemType : preferredItems) {
    if (eligibleItems.contains(preferredItemType)) {
      itemTypesToDisplay.push_back(preferredItemType);
    }
  }

  // If necessary, add additional items to have at least 2 and a maximum of 3
  // items in `itemsToDisplay`.
  for (BestFeaturesItemType replacementItemType : eligibleItems) {
    if (itemTypesToDisplay.size() > 2) {
      break;
    }
    if (std::find(itemTypesToDisplay.begin(), itemTypesToDisplay.end(),
                  replacementItemType) == itemTypesToDisplay.end()) {
      itemTypesToDisplay.push_back(replacementItemType);
    }
  }

  NSMutableArray<BestFeaturesItem*>* itemsToDisplay = [NSMutableArray array];
  for (BestFeaturesItemType type : itemTypesToDisplay) {
    [itemsToDisplay addObject:[[BestFeaturesItem alloc] initWithType:type]];
  }

  return itemsToDisplay;
}

// Returns the default items for each Welcome Back variation.
- (std::vector<BestFeaturesItemType>)preferredItems {
  using enum WelcomeBackScreenVariationType;
  using enum BestFeaturesItemType;

  switch (GetWelcomeBackScreenVariationType()) {
    case kBasicsWithLockedIncognitoTabs:
      return {kLensSearch, kEnhancedSafeBrowsing, kLockedIncognitoTabs};
    case kBasicsWithPasswords:
      return {kLensSearch, kEnhancedSafeBrowsing, kSaveAndAutofillPasswords};
    case kProductivityAndShopping:
      return {kTabGroups, kLockedIncognitoTabs, kPriceTrackingAndInsights};
    case kSignInBenefits:
      return {kLensSearch, kEnhancedSafeBrowsing,
              kAutofillPasswordsInOtherApps};
    case kDisabled:
      NOTREACHED();
  }
}

@end
