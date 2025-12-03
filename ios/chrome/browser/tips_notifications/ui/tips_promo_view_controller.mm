// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/tips_promo_view_controller.h"

#import "base/check.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Custom spacing added between the animation and the title.
const CGFloat kCustomSpacingAfterAnimation = 30;

// The height of the animation, as a percentage of the whole view.
const CGFloat kAnimationHeightPercent = 0.5;

}  // namespace

@implementation TipsPromoViewController {
  id<LottieAnimation> _animationViewWrapper;
  id<LottieAnimation> _animationViewWrapperDarkMode;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIStackView* contentStack = [self createContentStack];
  [self.contentView addSubview:contentStack];

  [NSLayoutConstraint activateConstraints:@[
    [_animationViewWrapper.animationView.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kAnimationHeightPercent],
  ]];
  AddSameConstraints(contentStack, self.contentView);

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(updateAnimation)];
}

#pragma mark - Private

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  id<LottieAnimation> animationWrapper =
      ios::provider::GenerateLottieAnimation(config);
  [animationWrapper setDictionaryTextProvider:self.animationTextProvider];
  return animationWrapper;
}

// Returns a stack view containing the animation image, the title, and the
// subtitle.
- (UIStackView*)createContentStack {
  UIStackView* stack = [[UIStackView alloc] init];

  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.alignment = UIStackViewAlignmentFill;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = UIStackViewSpacingUseSystem;

  _animationViewWrapper = [self createAnimation:self.animationName];
  UIView* animation = _animationViewWrapper.animationView;
  animation.translatesAutoresizingMaskIntoConstraints = NO;
  animation.contentMode = UIViewContentModeScaleAspectFit;
  [stack addArrangedSubview:animation];
  [stack setCustomSpacing:kCustomSpacingAfterAnimation afterView:animation];
  if (self.animationNameDarkMode) {
    CHECK(!self.lightModeColorProvider && !self.darkModeColorProvider);
    _animationViewWrapperDarkMode =
        [self createAnimation:self.animationNameDarkMode];
    UIView* animationDarkMode = _animationViewWrapperDarkMode.animationView;
    animationDarkMode.translatesAutoresizingMaskIntoConstraints = NO;
    animationDarkMode.contentMode = UIViewContentModeScaleAspectFit;
    [stack addArrangedSubview:animationDarkMode];
    [stack setCustomSpacing:kCustomSpacingAfterAnimation
                  afterView:animationDarkMode];
    AddSameConstraints(animation, animationDarkMode);
  } else {
    [_animationViewWrapper play];
  }
  [self updateAnimation];

  if (self.titleText.length > 0) {
    UILabel* title = [self createLabel:self.titleText
                                  font:GetFRETitleFont(UIFontTextStyleTitle2)
                                 color:kTextPrimaryColor];
    [stack addArrangedSubview:title];
  }

  if (self.subtitleText.length > 0) {
    UILabel* subtitle =
        [self createLabel:self.subtitleText
                     font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                    color:kTextSecondaryColor];
    [stack addArrangedSubview:subtitle];
  }

  return stack;
}

// Creates a label with the given  string, font, and color.
- (UILabel*)createLabel:(NSString*)text
                   font:(UIFont*)font
                  color:(NSString*)colorName {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.text = text;
  label.numberOfLines = 0;
  label.font = font;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:colorName];
  label.textAlignment = NSTextAlignmentCenter;
  return label;
}

// Updates the animation for dark/light mode.
- (void)updateAnimation {
  BOOL dark =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  if (self.lightModeColorProvider) {
    if (dark) {
      [self updateAnimationWithColorProvider:self.darkModeColorProvider];
    } else {
      [self updateAnimationWithColorProvider:self.lightModeColorProvider];
    }
  } else {
    [_animationViewWrapper stop];
    [_animationViewWrapperDarkMode stop];
    _animationViewWrapper.animationView.hidden = dark;
    _animationViewWrapperDarkMode.animationView.hidden = !dark;
    if (dark) {
      [_animationViewWrapperDarkMode play];
    } else {
      [_animationViewWrapper play];
    }
  }
}

// Updates the _animationViewWrapper with the colors from `colorProvider`.
- (void)updateAnimationWithColorProvider:
    (NSDictionary<NSString*, UIColor*>*)colorProvider {
  for (NSString* keypath in colorProvider.allKeys) {
    [_animationViewWrapper setColorValue:colorProvider[keypath]
                              forKeypath:keypath];
  }
}

@end
