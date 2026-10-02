// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_view_controller.h"

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the Search What You See Lottie.
NSString* const kAnimationName = @"search_what_you_see_promo";

// The accessibility identifier for the Search What You See view.
NSString* const kSearchWhatYouSeePromoAXID = @"kSearchWhatYouSeePromoAXID";

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* colorProvider() {
  return @{
    @"grouped_primary_background_color" :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    @"tertiary_background_color" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @"blue_color" : [UIColor colorNamed:kBlueColor],
  };
}

}  // namespace

@implementation SearchWhatYouSeePromoViewController

- (instancetype)init {
  ButtonStackConfiguration* buttonStackConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonStackConfiguration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_SHOW_ME_HOW_ACTION);

  AnimationBrickConfiguration* animationConfiguration =
      [[AnimationBrickConfiguration alloc] init];
  animationConfiguration.animationName = kAnimationName;
  animationConfiguration.lightModeColorProvider = colorProvider();
  animationConfiguration.darkModeColorProvider = colorProvider();
  animationConfiguration.useLegacyDarkMode = NO;

  TitleBrickConfiguration* titleConfiguration =
      [[TitleBrickConfiguration alloc] init];
  titleConfiguration.title =
      l10n_util::GetNSString(IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_TITLE);
  titleConfiguration.subtitle =
      l10n_util::GetNSString(IDS_IOS_SEARCH_WHAT_YOU_SEE_TIPS_PROMO_SUBTITLE);
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

  self.view.accessibilityIdentifier = kSearchWhatYouSeePromoAXID;
}

@end
