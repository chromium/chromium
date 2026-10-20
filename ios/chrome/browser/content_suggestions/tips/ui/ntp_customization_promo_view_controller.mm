// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_customization_promo_view_controller.h"

#import <algorithm>

#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/bricks/animation_brick.h"
#import "ios/chrome/browser/shared/ui/bricks/title_brick.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Lottie animation asset names.
NSString* const kAnimationName = @"ntp_customization";

// Accessibility identifier for the promo view.
NSString* const kNTPCustomizationPromoAccessibilityID =
    @"kNTPCustomizationPromoAccessibilityID";

// Layout constants for the animation container.
constexpr CGFloat kAnimationHeight = 180.0;
constexpr CGFloat kAnimationCornerRadius = 16.0;
constexpr CGFloat kContentTopPadding = 16.0;
constexpr CGFloat kTitleTopSpacing = 16.0;

// Creates the ButtonStackConfiguration for the bottom sheet.
ButtonStackConfiguration* CreateButtonStackConfiguration() {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_NTP_CUSTOMIZATION_PROMO_PRIMARY_BUTTON_TITLE);
  configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_NTP_CUSTOMIZATION_PROMO_SECONDARY_BUTTON_TITLE);
  return configuration;
}

// Returns the color provider for the animation.
NSDictionary<NSString*, UIColor*>* ColorProvider() {
  return @{
    kGroupedSecondaryBackgroundColor :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    kNewTabPageBackgroundColor :
        [UIColor colorNamed:kNewTabPageBackgroundColor],
    kNTPQuickActionChipColor : [UIColor colorNamed:kNTPQuickActionChipColor],
    kPrimaryBackgroundColor : [UIColor colorNamed:kPrimaryBackgroundColor],
  };
}

}  // namespace

@implementation NTPCustomizationPromoViewController {
  // Top constraint for the title when the animation is visible (portrait).
  NSLayoutConstraint* _titleTopToAnimationConstraint;
  // Top constraint for the title when the animation is hidden (landscape).
  NSLayoutConstraint* _titleTopToContentConstraint;
}

- (instancetype)init {
  return [super initWithConfiguration:CreateButtonStackConfiguration()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.scrollEnabled = NO;
  self.showsVerticalScrollIndicator = NO;
  self.showsGradientView = NO;
  [super viewDidLoad];

  self.view.accessibilityIdentifier = kNTPCustomizationPromoAccessibilityID;

  AnimationBrickConfiguration* animationConfig =
      [[AnimationBrickConfiguration alloc] init];
  animationConfig.animationName = kAnimationName;
  animationConfig.useLegacyDarkMode = NO;
  animationConfig.lightModeColorProvider = ColorProvider();
  animationConfig.darkModeColorProvider = ColorProvider();
  animationConfig.animationBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  AnimationBrick* animationBrick =
      [[AnimationBrick alloc] initWithConfiguration:animationConfig];
  animationBrick.translatesAutoresizingMaskIntoConstraints = NO;
  animationBrick.layer.cornerRadius = kAnimationCornerRadius;
  animationBrick.clipsToBounds = YES;
  [self.contentView addSubview:animationBrick];

  TitleBrickConfiguration* titleConfig = [[TitleBrickConfiguration alloc] init];
  titleConfig.title =
      l10n_util::GetNSString(IDS_IOS_NTP_CUSTOMIZATION_PROMO_TITLE);
  titleConfig.subtitle =
      l10n_util::GetNSString(IDS_IOS_NTP_CUSTOMIZATION_PROMO_SUBTITLE);
  titleConfig.titleStyle = UIFontTextStyleTitle2;

  TitleBrick* titleBrick =
      [[TitleBrick alloc] initWithConfiguration:titleConfig];
  titleBrick.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:titleBrick];

  _titleTopToAnimationConstraint =
      [titleBrick.topAnchor constraintEqualToAnchor:animationBrick.bottomAnchor
                                           constant:kTitleTopSpacing];
  _titleTopToContentConstraint =
      [titleBrick.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kContentTopPadding];

  [NSLayoutConstraint activateConstraints:@[
    [animationBrick.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                             constant:kContentTopPadding],
    [animationBrick.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [animationBrick.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [animationBrick.heightAnchor constraintEqualToConstant:kAnimationHeight],
    [titleBrick.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [titleBrick.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [titleBrick.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
  ]];

  [self updateConstraintsForTraitCollection:self.traitCollection];
}

#pragma mark - ButtonStackViewController

- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  __weak __typeof(self) weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf
        detentHeightForTraitCollection:context.containerTraitCollection
                    maximumDetentValue:context.maximumDetentValue];
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:@"preferred_height"
                        resolver:resolver];
}

#pragma mark - Private

// Updates the title top constraint for `traitCollection` and returns the
// resolved detent height.
- (CGFloat)detentHeightForTraitCollection:(UITraitCollection*)traitCollection
                       maximumDetentValue:(CGFloat)maximumDetentValue {
  [self updateConstraintsForTraitCollection:traitCollection];
  return std::min([self preferredHeightForContent], maximumDetentValue);
}

// Pins the title below the animation in regular height, or to the top of
// `contentView` in compact height.
- (void)updateConstraintsForTraitCollection:
    (UITraitCollection*)traitCollection {
  BOOL isCompactHeight =
      traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  _titleTopToAnimationConstraint.active = !isCompactHeight;
  _titleTopToContentConstraint.active = isCompactHeight;
}

@end
