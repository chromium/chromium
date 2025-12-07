// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/feature_highlight_screenshot_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Returns the secondary action string based on the current variation.
NSString* ConfigureSecondaryActionString() {
  if (IsWelcomeBackEnabled()) {
    return l10n_util::GetNSString(IDS_IOS_WELCOME_BACK_KEEP_BROWSING_BUTTON);
  }

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
    case kDisabled:
      NOTREACHED();
  }
}
}  // namespace

@interface FeatureHighlightScreenshotViewController ()

@end

@implementation FeatureHighlightScreenshotViewController {
  // The item that the promo is configured to present.
  BestFeaturesItem* _bestFeaturesItem;
}

- (instancetype)initWithFeatureHighlightItem:
    (BestFeaturesItem*)bestFeaturesItem {
  self = [super init];
  if (self) {
    _bestFeaturesItem = bestFeaturesItem;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleString = _bestFeaturesItem.title;
  self.subtitleString = _bestFeaturesItem.subtitle;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);
  self.secondaryActionString = ConfigureSecondaryActionString();
  self.animationName = _bestFeaturesItem.animationName;
  if (_bestFeaturesItem.lightModeColorProvider) {
    self.useLegacyDarkMode = NO;
    self.lightModeColorProvider = _bestFeaturesItem.lightModeColorProvider;
    self.darkModeColorProvider = _bestFeaturesItem.darkModeColorProvider;
  } else {
    self.animationNameDarkMode =
        [_bestFeaturesItem.animationName stringByAppendingString:@"_darkmode"];
  }
  self.animationTextProvider = _bestFeaturesItem.textProvider;
  [super viewDidLoad];
  [self.view setBackgroundColor:[UIColor colorNamed:kSecondaryBackgroundColor]];
}

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
