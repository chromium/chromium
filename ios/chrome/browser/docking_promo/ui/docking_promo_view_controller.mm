// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the Docking Promo.
NSString* const kAnimationName = @"docking_promo";

// The accessibility identifier for the Docking Promo view.
NSString* const kDockingPromoAccessibilityId = @"kDockingPromoAccessibilityId";

// The keypath for the "Edit Home Screen" animation text in the
// `animationTextProvider` dictionary.
NSString* const kEditHomeScreenKeypath = @"edit_home_screen";

// Returns the title string. The returned string may vary depending on
// active field trials or feature configurations.
NSString* GetTitleString() {
  if (!IsDockingPromoV2Enabled()) {
    return l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
  }

  std::string param = GetFieldTrialParamValueByFeature(
      kIOSDockingPromoV2, kIOSDockingPromoV2VariationParam);

  if (param == kIOSDockingPromoV2VariationHeader1) {
    return l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE_0);
  } else if (param == kIOSDockingPromoV2VariationHeader2) {
    return l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE_1);
  } else if (param == kIOSDockingPromoV2VariationHeader3) {
    return l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE_2);
  }
  return l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
}

// Returns the subtitle string. The returned string may vary depending on
// active field trials or feature configurations.
NSString* GetSubtitleString() {
  if (!IsDockingPromoV2Enabled()) {
    return nil;
  }

  std::string param = GetFieldTrialParamValueByFeature(
      kIOSDockingPromoV2, kIOSDockingPromoV2VariationParam);

  if (param == kIOSDockingPromoV2VariationHeader1 ||
      param == kIOSDockingPromoV2VariationHeader2) {
    return
        [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad
            ? l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SUBTITLE_IPAD)
            : l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SUBTITLE);
  }
  return nil;
}

}  // namespace

@implementation DockingPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.useLegacyDarkMode = NO;
  self.animationName = kAnimationName;
  self.animationBackgroundColor = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        return (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
                   ? [UIColor colorNamed:kBackgroundColor]
                   : [UIColor colorNamed:kGrey100Color];
      }];
  self.titleString = GetTitleString();
  self.subtitleString = GetSubtitleString();
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_PRIMARY_BUTTON_TITLE);

  // Set the text localization.
  NSString* editHomeScreenTitle = l10n_util::GetNSString(
      IDS_IOS_DOCKING_EDIT_HOME_SCREEN_LOTTIE_INSTRUCTION);
  self.animationTextProvider = @{kEditHomeScreenKeypath : editHomeScreenTitle};

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kDockingPromoAccessibilityId;

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(configureAnimationColors)];
  [self configureAnimationColors];
}

#pragma mark - Private

// Configures the animation with semantic and custom colors.
- (void)configureAnimationColors {
  ConfigureAnimationSemanticColors(self.animationViewWrapper);
}

@end
