// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kLensUserEducationLightMode = @"lens_usered_lightmode";

NSString* const kLensUserEducationDarkMode = @"lens_usered_darkmode";

// The approximate height of the items that have a fixed height:
// title,subtitle,learMoreLink and buttons.
const CGFloat kDialogFixedItemsHeight = 280;

// The height of the animation, as a percentage of the whole view minus the
// fixed height items. By subtracting out the height of the items with a
// fixed height, and sizing the animationa based on what is left we can
// scale more accurately on larger and smaller screens.
const CGFloat kAnimationHeightPercent = 0.70;

}  // namespace

@implementation LensOverlayConsentViewController {
  id<LottieAnimation> _animationViewWrapper;
  BOOL _isAnimationPlaying;
  UIButton* _animationPlayerButton;
  UIView* _animationView;
}

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type  (i.e.
// LensOverlayConsentViewControllerDelegate extends
// PromoStyleViewControllerDelegate).
@dynamic delegate;

- (void)viewDidLoad {
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  // Avoid extra top spacing.
  self.subtitleBottomMargin = 0;
  self.headerImageBottomMargin = 0;

  UIStackView* contentStack = [self createContentStack];
  [self.specificContentView addSubview:contentStack];

  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_LENS_OVERLAY_CONSENT_ACCEPT_TERMS_BUTTON_TITLE);
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_LENS_OVERLAY_CONSENT_DENY_TERMS_BUTTON_TITLE);

  [super viewDidLoad];

  UILayoutGuide* contentLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:contentLayoutGuide];

  [NSLayoutConstraint activateConstraints:@[
    [contentLayoutGuide.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                       constant:-kDialogFixedItemsHeight],
    [_animationView.heightAnchor
        constraintEqualToAnchor:contentLayoutGuide.heightAnchor
                     multiplier:kAnimationHeightPercent],
    [self.specificContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:contentStack.heightAnchor],
  ]];
  AddSameConstraintsToSides(
      contentStack, self.specificContentView,
      LayoutSides::kTrailing | LayoutSides::kLeading | LayoutSides::kTop);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_animationViewWrapper play];
  _isAnimationPlaying = YES;
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.view);
}

- (CGSize)preferredContentSize {
  CGFloat fittingWidth = self.view.safeAreaLayoutGuide.layoutFrame.size.width;
  CGSize fittingSize =
      CGSizeMake(fittingWidth, UILayoutFittingCompressedSize.height);
  CGFloat height = [self.view systemLayoutSizeFittingSize:fittingSize].height;

  return CGSizeMake(fittingWidth, height);
}

#pragma mark - Private

- (UIStackView*)createContentStack {
  // Lottie animation.
  _animationViewWrapper =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
          ? [self createAnimation:kLensUserEducationDarkMode]
          : [self createAnimation:kLensUserEducationLightMode];

  _animationView = _animationViewWrapper.animationView;

  _animationView.translatesAutoresizingMaskIntoConstraints = NO;
  _animationView.contentMode = UIViewContentModeScaleAspectFit;

  _animationPlayerButton = [self newAnimationPlayerButton];

  [_animationPlayerButton addTarget:self
                             action:@selector(animationPlayerButtonTapped)
                   forControlEvents:UIControlEventTouchUpInside];

  [_animationView addSubview:_animationPlayerButton];

  [NSLayoutConstraint activateConstraints:@[
    [_animationPlayerButton.rightAnchor
        constraintEqualToAnchor:_animationView.rightAnchor
                       constant:-20],
    [_animationPlayerButton.bottomAnchor
        constraintEqualToAnchor:_animationView.bottomAnchor
                       constant:-20]
  ]];

  // Title/description labels.
  UILabel* titleLabel = [self
      createLabel:l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_TITLE)
             font:GetFRETitleFont(UIFontTextStyleTitle2)
            color:kTextPrimaryColor];

  UILabel* bodyLabel =
      [self createLabel:l10n_util::GetNSString(
                            IDS_IOS_LENS_OVERLAY_CONSENT_DESCRIPTION)
                   font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                  color:kLensOverlayConsentDialogDescriptionColor];

  // Clear `titleText` and `subtitleText` so that PromoStyleViewController does
  // not use them to create alternate title and subtitle labels.
  self.titleText = nil;
  self.subtitleText = nil;

  // Learn more link.
  __weak __typeof(self) weakSelf = self;
  UIButton* learnMoreLink =
      [self plainButtonWithTitle:l10n_util::GetNSString(
                                     IDS_IOS_LENS_OVERLAY_CONSENT_LEARN_MORE)
                   actionHandler:^(UIAction* action) {
                     [weakSelf.delegate didPressLearnMore];
                   }];

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _animationView, titleLabel, bodyLabel, learnMoreLink
  ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.alignment = UIStackViewAlignmentFill;
  stack.spacing = 20;
  [stack setCustomSpacing:8 afterView:titleLabel];
  [stack setCustomSpacing:8 afterView:bodyLabel];

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
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

- (UIButton*)plainButtonWithTitle:(NSString*)title
                    actionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.title = title;

  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];
  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  button.titleLabel.textAlignment = NSTextAlignmentCenter;

  return button;
}

- (UIButton*)newAnimationPlayerButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
  button.tintColor =
      [UIColor colorNamed:kLensOverlayConsentDialogAnimationPlayerButtonColor];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:22
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [button setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];

  [button setImage:DefaultSymbolWithPointSize(kPauseButton, 22)
          forState:UIControlStateNormal];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;

  return button;
}

- (void)animationPlayerButtonTapped {
  _isAnimationPlaying = !_isAnimationPlaying;

  if (_isAnimationPlaying) {
    [_animationPlayerButton
        setImage:DefaultSymbolWithPointSize(kPauseButton, 22)
        forState:UIControlStateNormal];
    [_animationViewWrapper play];
  } else {
    [_animationPlayerButton setImage:DefaultSymbolWithPointSize(kPlayButton, 22)
                            forState:UIControlStateNormal];
    [_animationViewWrapper pause];
  }
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = -1;  // Always loop.
  return ios::provider::GenerateLottieAnimation(config);
}

@end
