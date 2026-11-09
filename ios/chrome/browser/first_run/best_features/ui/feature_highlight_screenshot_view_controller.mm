// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/ui/feature_highlight_screenshot_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "ios/chrome/browser/first_run/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Returns the secondary action string based on the current variation.
NSString* ConfigureSecondaryActionString() {
  using enum first_run::BestFeaturesScreenVariationType;
  switch (first_run::GetBestFeaturesScreenVariationType()) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      // Best Features screen is last in the FRE sequence
      return l10n_util::GetNSString(
          IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);
    case kGeneralScreenBeforeDBPromo:
      // Best Features screen is not last in the FRE sequence
      return l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_CONTINUE_BUTTON);
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
    case kBestOfApp:
      // kBestOfApp does not have a secondary action string. It will be
      // presented from the `FeatureHighlightAnimatedViewController`.
    case kDisabled:
      NOTREACHED();
  }
}
}  // namespace

@implementation FeatureHighlightScreenshotViewController {
  // The item that the promo is configured to present.
  BestFeaturesItem* _bestFeaturesItem;
}

- (instancetype)initWithFeatureHighlightItem:
    (BestFeaturesItem*)bestFeaturesItem {
  ButtonStackConfiguration* buttonStackConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonStackConfiguration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);
  buttonStackConfiguration.secondaryActionString =
      ConfigureSecondaryActionString();

  AnimationBrickConfiguration* animationConfiguration =
      [[AnimationBrickConfiguration alloc] init];
  animationConfiguration.animationName = bestFeaturesItem.animationName;
  if (bestFeaturesItem.lightModeColorProvider) {
    animationConfiguration.useLegacyDarkMode = NO;
    animationConfiguration.lightModeColorProvider =
        bestFeaturesItem.lightModeColorProvider;
    animationConfiguration.darkModeColorProvider =
        bestFeaturesItem.darkModeColorProvider;
  } else {
    animationConfiguration.darkAnimationName =
        [bestFeaturesItem.animationName stringByAppendingString:@"_darkmode"];
  }
  animationConfiguration.animationBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  animationConfiguration.animationTextProvider = bestFeaturesItem.textProvider;

  TitleBrickConfiguration* titleConfiguration =
      [[TitleBrickConfiguration alloc] init];
  titleConfiguration.title = bestFeaturesItem.title;
  titleConfiguration.subtitle = bestFeaturesItem.subtitle;
  titleConfiguration.titleStyle = UIFontTextStyleTitle2;

  AnimatedPromoConfiguration* config = [[AnimatedPromoConfiguration alloc]
      initWithAnimationBrickConfiguration:animationConfiguration
                 buttonStackConfiguration:buttonStackConfiguration
                  titleBrickConfiguration:titleConfiguration
                           underTitleView:nil];

  self = [super initWithConfiguration:config];
  if (self) {
    _bestFeaturesItem = bestFeaturesItem;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  if (self.isMovingFromParentViewController) {
    // Back button was pressed.
    base::UmaHistogramEnumeration(
        BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
        BestFeaturesDetailScreenActionType::kNavigateBack);
  }
}

@end
