// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/showcase_model.h"

#import "base/check.h"
#import "ios/showcase/core/showcase_model_buildflags.h"

namespace {

// Validates whether all classes referenced by name in |row| can be loaded
// using Objective-C reflection.
BOOL IsShowcaseModelRowValid(showcase::ModelRow* row) {
  static NSArray<NSString*>* const keys =
      @[ showcase::kClassForInstantiationKey ];

  BOOL valid = YES;
  for (NSString* key in keys) {
    if (!NSClassFromString(row[key])) {
      NSLog(@"Can't load class: %@", row[key]);
      valid = NO;
    }
  }
  return valid;
}

// Validates whether all row in |model| are valid.
BOOL IsShowcaseModelValid(NSArray<showcase::ModelRow*>* model) {
  BOOL valid = YES;
  for (showcase::ModelRow* row in model) {
    if (!IsShowcaseModelRowValid(row))
      valid = NO;
  }
  return valid;
}

}  // namespace

@implementation ShowcaseModel

// Insert additional rows in this array. All rows will be sorted upon
// import into Showcase.
// |kShowcaseClassForDisplayKey| and |kShowcaseClassForInstantiationKey| are
// required. |kShowcaseUseCaseKey| is optional.
+ (NSArray<showcase::ModelRow*>*)model {
  NSArray<showcase::ModelRow*>* model = @[
    @{
      showcase::kClassForDisplayKey : @"ConsentViewController",
      showcase::kClassForInstantiationKey : @"ConsentViewController",
      showcase::kUseCaseKey : @"Credential Provider Consent UI",
    },
    @{
      showcase::kClassForDisplayKey : @"LaunchScreenViewController",
      showcase::kClassForInstantiationKey : @"LaunchScreenViewController",
      showcase::kUseCaseKey : @"Launch screen",
    },
    @{
      showcase::kClassForDisplayKey : @"EnterpriseLoadScreenViewController",
      showcase::
      kClassForInstantiationKey : @"EnterpriseLoadScreenViewController",
      showcase::kUseCaseKey : @"Enterprise loading screen",
    },
    @{
      showcase::kClassForDisplayKey : @"EmptyCredentialsViewController",
      showcase::kClassForInstantiationKey : @"EmptyCredentialsViewController",
      showcase::kUseCaseKey : @"Credential Provider Empty Credentials UI",
    },
    @{
      showcase::kClassForDisplayKey : @"StaleCredentialsViewController",
      showcase::kClassForInstantiationKey : @"StaleCredentialsViewController",
      showcase::kUseCaseKey : @"Credential Provider Stale Credentials UI",
    },
#if BUILDFLAG(SHOWCASE_CREDENTIAL_PROVIDER_ENABLED)
    @{
      showcase::kClassForDisplayKey : @"CredentialListViewController",
      showcase::kClassForInstantiationKey : @"SCCredentialListCoordinator",
      showcase::kUseCaseKey : @"Credential Provider Credentials List UI",
    },
#endif
    @{
      showcase::kClassForDisplayKey : @"SettingsViewController",
      showcase::kClassForInstantiationKey : @"SCSettingsCoordinator",
      showcase::kUseCaseKey : @"Main settings screen",
    },
    @{
      showcase::kClassForDisplayKey : @"UITableViewCell",
      showcase::kClassForInstantiationKey : @"UIKitTableViewCellViewController",
      showcase::kUseCaseKey : @"UIKit Table Cells",
    },
    @{
      showcase::kClassForDisplayKey : @"TextBadgeView",
      showcase::kClassForInstantiationKey : @"SCTextBadgeViewController",
      showcase::kUseCaseKey : @"Text badge",
    },
    @{
      showcase::kClassForDisplayKey : @"BubbleViewController",
      showcase::kClassForInstantiationKey : @"SCBubbleCoordinator",
      showcase::kUseCaseKey : @"Bubble",
    },
    @{
      showcase::kClassForDisplayKey : @"SideSwipeBubbleView",
      showcase::kClassForInstantiationKey : @"SCSideSwipeBubbleViewController",
      showcase::kUseCaseKey : @"Side Swipe Bubble",
    },
    @{
      showcase::kClassForDisplayKey : @"RecentTabsTableViewController",
      showcase::kClassForInstantiationKey : @"SCDarkThemeRecentTabsCoordinator",
      showcase::kUseCaseKey : @"Dark theme recent tabs",
    },
    @{
      showcase::kClassForDisplayKey : @"OmniboxPopupViewController",
      showcase::kClassForInstantiationKey : @"SCOmniboxPopupCoordinator",
      showcase::kUseCaseKey : @"Omnibox popup table view",
    },
    @{
      showcase::kClassForDisplayKey : @"InfobarBannerViewController",
      showcase::kClassForInstantiationKey : @"SCInfobarBannerCoordinator",
      showcase::kUseCaseKey : @"Infobar Banner",
    },
    @{
      showcase::kClassForDisplayKey : @"InfobarBannerViewController",
      showcase::
      kClassForInstantiationKey : @"SCInfobarBannerNoModalCoordinator",
      showcase::kUseCaseKey : @"Infobar Banner No Modal",
    },
    @{
      showcase::kClassForDisplayKey : @"AlertController",
      showcase::kClassForInstantiationKey : @"SCAlertCoordinator",
      showcase::kUseCaseKey : @"Alert",
    },
    @{
      showcase::kClassForDisplayKey : @"BadgeViewController",
      showcase::kClassForInstantiationKey : @"SCBadgeCoordinator",
      showcase::kUseCaseKey : @"Badge View",
    },
    @{
      showcase::kClassForDisplayKey : @"SaveCardModalViewController",
      showcase::
      kClassForInstantiationKey : @"SCInfobarModalSaveCardCoordinator",
      showcase::kUseCaseKey : @"Save Card Modal",
    },
    @{
      showcase::kClassForDisplayKey : @"SCIncognitoReauthViewController",
      showcase::kClassForInstantiationKey : @"SCIncognitoReauthViewController",
      showcase::kUseCaseKey : @"Incognito Reauth Blocker",
    },
    @{
      showcase::kClassForDisplayKey : @"DefaultBrowserPromoViewController",
      showcase::
      kClassForInstantiationKey : @"SCDefaultBrowserFullscreenPromoCoordinator",
      showcase::kUseCaseKey : @"Default Browser Fullscreen Promo UI",
    },
    @{
      showcase::kClassForDisplayKey : @"SCFirstRunHeroScreenViewController",
      showcase::kClassForInstantiationKey : @"SCFirstRunHeroScreenCoordinator",
      showcase::kUseCaseKey : @"New FRE hero screen example",
    },
    @{
      showcase::kClassForDisplayKey : @"SCFirstRunDefaultScreenViewController",
      showcase::
      kClassForInstantiationKey : @"SCFirstRunDefaultScreenCoordinator",
      showcase::kUseCaseKey : @"New FRE default screen example",
    },
    @{
      showcase::
      kClassForDisplayKey : @"SCFirstRunScrollingScreenViewController",
      showcase::
      kClassForInstantiationKey : @"SCFirstRunScrollingScreenCoordinator",
      showcase::kUseCaseKey : @"New FRE screen with scrolling example",
    },
    @{
      showcase::kClassForDisplayKey : @"LinkPreviewViewController",
      showcase::kClassForInstantiationKey : @"SCLinkPreviewCoordinator",
      showcase::kUseCaseKey : @"Link Preview",
    },
    @{
      showcase::kClassForDisplayKey : @"SCFollowViewController",
      showcase::kClassForInstantiationKey : @"SCFollowViewController",
      showcase::kUseCaseKey : @"Web Channels First Follow and Follow Mgmt UI",
    },
    @{
      showcase::kClassForDisplayKey : @"SCFeedSignInPromoViewController",
      showcase::kClassForInstantiationKey : @"SCFeedSignInPromoCoordinator",
      showcase::kUseCaseKey :
          @"Sign in promo half sheet for feed personalization menu options",
    },
    @{
      showcase::kClassForDisplayKey : @"SCUserPolicyPromptViewController",
      showcase::kClassForInstantiationKey : @"SCUserPolicyPromptCoordinator",
      showcase::kUseCaseKey : @"User Policy prompt half sheet",
    },
  ];
  DCHECK(IsShowcaseModelValid(model));
  return model;
}

@end
