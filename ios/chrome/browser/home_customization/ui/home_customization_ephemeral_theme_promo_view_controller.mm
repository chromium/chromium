// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_view_controller.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Animation card dimensions and styling constants.
constexpr CGFloat kCardCornerRadius = 24.0;
constexpr CGFloat kCardMaxWidth = 327.0;
constexpr CGFloat kCardMaxHeight = 180.0;
constexpr CGFloat kSpacingBeforeImage = 24.0;
constexpr CGFloat kSpacingAfterImage = 24.0;
constexpr CGFloat kSpacingTitleToSubtitle = 8.0;

}  // namespace

@implementation HomeCustomizationEphemeralThemePromoViewController {
  NSString* _animationAssetName;
  NSBundle* _animationBundle;
  NSDictionary<NSString*, UIColor*>* _lightModeColorProvider;
  NSDictionary<NSString*, UIColor*>* _darkModeColorProvider;

  UIView* _animationCardContainer;
  id<LottieAnimation> _currentLottieAnimation;
}

#pragma mark - Initializers

- (instancetype)init {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_PRIMARY_BUTTON_TEXT);
  configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_SECONDARY_BUTTON_TEXT);
  return [super initWithConfiguration:configuration];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleString = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_TITLE);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_SUBTITLE);
  self.customSpacingBeforeImage = kSpacingBeforeImage;
  self.customSpacing = kSpacingTitleToSubtitle;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleTextStyle = UIFontTextStyleSubheadline;
  self.topAlignedLayout = YES;
  self.scrollEnabled = YES;

  self.aboveTitleView = [self createAnimationCardView];
  [self setupAnimationView];

  [super viewDidLoad];

  self.titleLabel.font =
      PreferredFontForTextStyle(self.titleTextStyle, UIFontWeightSemibold);

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(updateAnimationColors)];

  self.view.accessibilityIdentifier =
      kHomeCustomizationEphemeralThemePromoAccessibilityIdentifier;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_currentLottieAnimation play];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [_currentLottieAnimation stop];
}

#pragma mark - HomeCustomizationEphemeralThemePromoConsumer

- (void)setAnimationAssetName:(NSString*)animationAssetName
                       bundle:(NSBundle*)bundle {
  _animationAssetName = [animationAssetName copy];
  _animationBundle = bundle;
  if (self.isViewLoaded) {
    [self setupAnimationView];
  }
}

- (void)setLightModeColorProvider:
            (NSDictionary<NSString*, UIColor*>*)lightModeColorProvider
            darkModeColorProvider:
                (NSDictionary<NSString*, UIColor*>*)darkModeColorProvider {
  _lightModeColorProvider = [lightModeColorProvider copy];
  _darkModeColorProvider = [darkModeColorProvider copy];
  if (self.isViewLoaded) {
    [self updateAnimationColors];
  }
}

#pragma mark - Private

// Creates the rounded card view that hosts the Lottie animation.
- (UIView*)createAnimationCardView {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  _animationCardContainer = [[UIView alloc] init];
  _animationCardContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _animationCardContainer.layer.cornerRadius = kCardCornerRadius;
  _animationCardContainer.clipsToBounds = YES;
  _animationCardContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  [containerView addSubview:_animationCardContainer];

  NSLayoutConstraint* widthConstraint = [_animationCardContainer.widthAnchor
      constraintEqualToAnchor:containerView.widthAnchor];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [_animationCardContainer.topAnchor
        constraintEqualToAnchor:containerView.topAnchor],
    [_animationCardContainer.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor
                       constant:-(kSpacingAfterImage -
                                  kSpacingTitleToSubtitle)],
    [_animationCardContainer.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],
    [_animationCardContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:containerView.leadingAnchor],
    [_animationCardContainer.trailingAnchor
        constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
    [_animationCardContainer.widthAnchor
        constraintLessThanOrEqualToConstant:kCardMaxWidth],
    widthConstraint,
    [_animationCardContainer.heightAnchor
        constraintEqualToAnchor:_animationCardContainer.widthAnchor
                     multiplier:(kCardMaxHeight / kCardMaxWidth)],
  ]];

  return containerView;
}

// Sets up the Lottie animation configuration, creates the animation view, and
// configures its layout constraints within the card container.
- (void)setupAnimationView {
  if (!_animationCardContainer || _animationAssetName.length == 0) {
    return;
  }

  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = _animationAssetName;
  config.bundle = _animationBundle;
  config.shouldLoop = YES;

  [_currentLottieAnimation.animationView removeFromSuperview];
  _currentLottieAnimation = ios::provider::GenerateLottieAnimation(config);
  if (!_currentLottieAnimation) {
    return;
  }

  [self updateAnimationColors];

  _currentLottieAnimation.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _currentLottieAnimation.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  [_animationCardContainer addSubview:_currentLottieAnimation.animationView];
  AddSameConstraints(_currentLottieAnimation.animationView,
                     _animationCardContainer);
}

// Configures dynamic colors on the active Lottie animation.
- (void)updateAnimationColors {
  if (!_currentLottieAnimation) {
    return;
  }

  if (!_lightModeColorProvider || !_darkModeColorProvider) {
    return;
  }

  for (NSString* key in _lightModeColorProvider.allKeys) {
    UIColor* lightColor = _lightModeColorProvider[key];
    UIColor* darkColor = _darkModeColorProvider[key];
    ConfigureAnimationCustomColor(_currentLottieAnimation, key, lightColor,
                                  darkColor);
  }
}

@end
