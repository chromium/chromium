// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/tips_promo_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Custom spacing added between the animation and the title.
const CGFloat kCustomSpacingAfterAnimation = 30;

// The approxomate height of the items that have a fixed height: title,
// subtitle, and buttons.
const CGFloat kFixedItemsHeight = 200;

// The height of the animation, as a percentage of the whole view minus the
// fixed height items. By subtracting out the height of the items with a
// fixed height, and sizing the animationa based on what is left we can
// scale more accurately on larger and smaller screens.
const CGFloat kAnimationHeightPercent = 0.75;

}  // namespace

@implementation TipsPromoViewController {
  id<LottieAnimation> _animationViewWrapper;
  id<LottieAnimation> _animationViewWrapperDarkMode;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.delegate
                           action:@selector(didDismissViewController)];
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  UIStackView* contentStack = [self createContentStack];
  [self.specificContentView addSubview:contentStack];
  [super viewDidLoad];

  UILayoutGuide* contentLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:contentLayoutGuide];

  [NSLayoutConstraint activateConstraints:@[
    [contentLayoutGuide.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                       constant:-kFixedItemsHeight],
    [_animationViewWrapper.animationView.heightAnchor
        constraintEqualToAnchor:contentLayoutGuide.heightAnchor
                     multiplier:kAnimationHeightPercent],
    [self.specificContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:contentStack.heightAnchor],
  ]];
  AddSameConstraintsToSides(
      contentStack, self.specificContentView,
      LayoutSides::kTrailing | LayoutSides::kLeading | LayoutSides::kTop);

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.self ]
                       withAction:@selector(updateAnimation)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    [self updateAnimation];
  }
}
#endif

#pragma mark - Private

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = -1;  // Always loop.
  id<LottieAnimation> animationWrapper =
      ios::provider::GenerateLottieAnimation(config);
  [animationWrapper setDictionaryTextProvider:self.animationTextProvider];
  return animationWrapper;
}

// Returns a stack view containing the animation image, the title, and the
// subtitle.
- (UIStackView*)createContentStack {
  _animationViewWrapper = [self createAnimation:self.animationName];
  UIView* animation = _animationViewWrapper.animationView;
  animation.translatesAutoresizingMaskIntoConstraints = NO;
  animation.contentMode = UIViewContentModeScaleAspectFit;
  _animationViewWrapperDarkMode =
      [self createAnimation:self.animationNameDarkMode];
  UIView* animationDarkMode = _animationViewWrapperDarkMode.animationView;
  animationDarkMode.translatesAutoresizingMaskIntoConstraints = NO;
  animationDarkMode.contentMode = UIViewContentModeScaleAspectFit;
  [self updateAnimation];

  UILabel* title = [self createLabel:self.titleText
                                font:GetFRETitleFont(UIFontTextStyleTitle2)
                               color:kTextPrimaryColor];
  UILabel* subtitle =
      [self createLabel:self.subtitleText
                   font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                  color:kTextSecondaryColor];
  // Clear `titleText` and `subtitleText` so that PromoStyleViewController does
  // not use them to create alternate title and subtitle labels.
  self.titleText = nil;
  self.subtitleText = nil;

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    animation,
    animationDarkMode,
    title,
    subtitle,
  ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.alignment = UIStackViewAlignmentFill;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = UIStackViewSpacingUseSystem;
  [stack setCustomSpacing:kCustomSpacingAfterAnimation afterView:animation];
  [stack setCustomSpacing:kCustomSpacingAfterAnimation
                afterView:animationDarkMode];
  AddSameConstraints(animation, animationDarkMode);
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

@end
