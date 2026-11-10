// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the Docking Promo.
NSString* const kAnimationName = @"docking_promo";

// The name of the animation used for the Docking Promo in dark mode.
NSString* const kAnimationNameDarkMode = @"docking_promo_darkmode";

// The accessibility identifier for the Docking Promo view.
NSString* const kDockingPromoAccessibilityId = @"kDockingPromoAccessibilityId";

// The keypath for the "Edit Home Screen" animation text in the
// `animationTextProvider` dictionary.
NSString* const kEditHomeScreenKeypath = @"edit_home_screen";

}  // namespace

@implementation DockingPromoViewController

- (instancetype)init {
  ButtonStackConfiguration* buttonStackConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonStackConfiguration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_PRIMARY_BUTTON_TITLE);

  AnimationBrickConfiguration* animationConfiguration =
      [[AnimationBrickConfiguration alloc] init];
  animationConfiguration.animationName = kAnimationName;
  animationConfiguration.darkAnimationName = kAnimationNameDarkMode;
  animationConfiguration.animationBackgroundColor = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        return (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
                   ? [UIColor colorNamed:kBackgroundColor]
                   : [UIColor colorNamed:kGrey100Color];
      }];

  // Set the text localization.
  NSString* editHomeScreenTitle = l10n_util::GetNSString(
      IDS_IOS_DOCKING_EDIT_HOME_SCREEN_LOTTIE_INSTRUCTION);
  animationConfiguration.animationTextProvider =
      @{kEditHomeScreenKeypath : editHomeScreenTitle};

  TitleBrickConfiguration* titleConfiguration =
      [[TitleBrickConfiguration alloc] init];
  titleConfiguration.title =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
  titleConfiguration.titleStyle = UIFontTextStyleTitle2;

  AnimatedPromoConfiguration* animatedPromoConfiguration =
      [[AnimatedPromoConfiguration alloc]
          initWithAnimationBrickConfiguration:animationConfiguration
                     buttonStackConfiguration:buttonStackConfiguration
                      titleBrickConfiguration:titleConfiguration
                               underTitleView:nil];

  return [super initWithConfiguration:animatedPromoConfiguration];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = kDockingPromoAccessibilityId;
}

@end
