// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_view_controller.h"

#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_constants.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
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

@implementation ContextualDefaultBrowserPromoViewController {
  NSString* _animationAssetName;
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
      IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_PRIMARY_BUTTON_TEXT);
  configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_SECONDARY_BUTTON_TEXT);
  return [super initWithConfiguration:configuration];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.customSpacingBeforeImage = kSpacingBeforeImage;
  self.customSpacing = kSpacingTitleToSubtitle;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleTextStyle = UIFontTextStyleSubheadline;
  self.topAlignedLayout = YES;
  self.scrollEnabled = YES;

  self.aboveTitleView = [self createAnimationCardView];
  [self setupAnimationView];

  [super viewDidLoad];

  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:self.titleTextStyle];
  UIFont* semiboldFont = [UIFont systemFontOfSize:descriptor.pointSize
                                           weight:UIFontWeightSemibold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:self.titleTextStyle];
  self.titleLabel.font = [fontMetrics scaledFontForFont:semiboldFont];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(updateAnimationColors)];

  self.view.accessibilityIdentifier =
      kContextualDefaultBrowserPromoAccessibilityIdentifier;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_currentLottieAnimation play];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [_currentLottieAnimation stop];
}

#pragma mark - ContextualDefaultBrowserPromoConsumer

- (void)setPromoTitle:(NSString*)promoTitle {
  self.titleString = promoTitle;
}

- (void)setPromoSubtitle:(NSString*)promoSubtitle {
  self.subtitleString = promoSubtitle;
}

- (void)setAnimationAssetName:(NSString*)animationAssetName {
  _animationAssetName = animationAssetName;
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
  config.shouldLoop = YES;

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
