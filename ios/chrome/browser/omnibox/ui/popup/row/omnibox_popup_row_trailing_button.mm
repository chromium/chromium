// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_trailing_button.h"

#import "base/check.h"
#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {
/// Size of the trailing button.
const CGFloat kTrailingButtonIconPointSizeMedium = 15.0f;

/// The animation view size for Medium content size.
const CGFloat kAimAnimationViewSizeMedium = 33.33f;

// The name of the animation for the AIM button.
NSString* const kAIMCircleAnimationLightMode = @"mia_circle_animation_no_glow";
NSString* const kAIMCircleAnimationDarkMode = @"mia_glowing_circle_animation";

}  // namespace

@implementation OmniboxPopupRowTrailingButton {
  /// The aim animation view.
  UIView* _aimAnimationView;
  /// The aim lottie animation.
  id<LottieAnimation> _aimAnimation;
  /// Width constraint for the aim animation view.
  NSLayoutConstraint* _aimAnimationWidthConstraint;
  /// Height constraint for the aim animation view.
  NSLayoutConstraint* _aimAnimationHeightConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveMemoryWarning)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
    if (@available(iOS 17, *)) {
      [self
          registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(traitCollectionDidChangeAction)];
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setTrailingIconType:(TrailingIconType)trailingIconType {
  if (trailingIconType == _trailingIconType) {
    return;
  }

  _trailingIconType = trailingIconType;
  [self removeSearchWithAimAnimationViewIfNeeded];
  [self updateButtonImageForCurrentState];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (_isHighlighted == isHighlighted) {
    return;
  }

  _isHighlighted = isHighlighted;
  [self updateTintColor];
}

- (CGSize)intrinsicContentSize {
  if (self.trailingIconType == TrailingIconType::kNone) {
    return CGSizeZero;
  }

  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);
  if (multiplier == 0) {
    multiplier = 1.0;
  }

  // Override intrinsicContentSize to match the animation size, ensuring
  // all trailing buttons are aligned.
  CGFloat size = kAimAnimationViewSizeMedium * multiplier;
  return CGSizeMake(size, size);
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self traitCollectionDidChangeAction];
  }
}
#endif

#pragma mark - Low memory warning

- (void)didReceiveMemoryWarning {
  // If the animation view is not on screen, we can safely purge it.
  if (!_aimAnimationView.window) {
    _aimAnimation = nil;
    _aimAnimationView = nil;
  }
}

#pragma mark - private

- (void)traitCollectionDidChangeAction {
  if (_aimAnimationView) {
    [self updateAimAnimationViewSize];
  }
  [self updateButtonImageForCurrentState];
  [self invalidateIntrinsicContentSize];
}

- (void)updateButtonImageForCurrentState {
  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);

  if (multiplier) {
    UIImage* icon;
    self.hidden = NO;
    switch (self.trailingIconType) {
      case TrailingIconType::kNone:
        self.accessibilityIdentifier = nil;
        self.hidden = YES;
        return;
      case TrailingIconType::kSearchWithAim:
        icon = MakeSymbolMonochrome(CustomSymbolWithPointSize(
            kMagnifyingglassSparkSymbol,
            kTrailingButtonIconPointSizeMedium * multiplier));
        self.accessibilityIdentifier =
            kOmniboxPopupRowSearchWithAimAccessibilityIdentifier;
        if (!_aimAnimationView) {
          [self setupSearchWithAimAnimationView];
        }
        break;
      case TrailingIconType::kRefineQuery:
        icon = DefaultSymbolWithPointSize(
            kRefineQuerySymbol,
            kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowAppendAccessibilityIdentifier;
        break;
      case TrailingIconType::kOpenExistingTab:
        icon = DefaultSymbolWithPointSize(
            kNavigateToTabSymbol,
            kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
        break;
    }

    if (self.trailingIconType != TrailingIconType::kSearchWithAim) {
      // `imageWithHorizontallyFlippedOrientation` is flipping the icon
      // automatically when the UI is RTL/LTR.
      icon = [icon imageWithHorizontallyFlippedOrientation];
      icon = [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }

    [self setImage:icon forState:UIControlStateNormal];
    [self updateTintColor];
  }
}

- (void)updateTintColor {
  if (self.isHighlighted) {
    self.tintColor = UIColor.whiteColor;
    return;
  }

  if (self.trailingIconType == TrailingIconType::kSearchWithAim) {
    self.tintColor = [UIColor colorNamed:kSolidBlackColor];
    return;
  }

  self.tintColor = [UIColor colorNamed:kBlueColor];
}

// Updates the aim animation view size based on the content size category.
- (void)updateAimAnimationViewSize {
  if (!_aimAnimationWidthConstraint) {
    return;
  }

  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);
  if (multiplier > 0) {
    CGFloat size = kAimAnimationViewSizeMedium * multiplier;
    _aimAnimationWidthConstraint.constant = size;
    _aimAnimationHeightConstraint.constant = size;
  }
}

// Setups the search with Aim animation.
- (void)setupSearchWithAimAnimationView {
  self.pointerInteractionEnabled = YES;
  self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();

  [self addSubview:self.aimAnimationView];
  self.aimAnimationView.userInteractionEnabled = NO;
  AddSameCenterConstraints(self.aimAnimationView, self);

  _aimAnimationWidthConstraint = [self.aimAnimationView.widthAnchor
      constraintEqualToConstant:kAimAnimationViewSizeMedium];
  _aimAnimationHeightConstraint = [self.aimAnimationView.heightAnchor
      constraintEqualToConstant:kAimAnimationViewSizeMedium];
  [NSLayoutConstraint activateConstraints:@[
    _aimAnimationWidthConstraint,
    _aimAnimationHeightConstraint,
  ]];

  [self updateAimAnimationViewSize];
  [self.aimLottieAnimation play];
}

// Removes the aim animation for the view.
- (void)removeSearchWithAimAnimationViewIfNeeded {
  if (self.trailingIconType == TrailingIconType::kSearchWithAim) {
    return;
  }

  [self.aimAnimationView removeFromSuperview];
  _aimAnimationView = nil;
}

// Creates an animation view for the AIM entry point.
- (UIView*)aimAnimationView {
  if (_aimAnimationView) {
    return _aimAnimationView;
  }

  _aimAnimationView = self.aimLottieAnimation.animationView;
  _aimAnimationView.translatesAutoresizingMaskIntoConstraints = NO;
  _aimAnimationView.contentMode = UIViewContentModeScaleAspectFit;

  return _aimAnimationView;
}

// Creates and returns the LottieAnimation for the AIM button.
- (id<LottieAnimation>)aimLottieAnimation {
  if (_aimAnimation) {
    return _aimAnimation;
  }
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
          ? kAIMCircleAnimationDarkMode
          : kAIMCircleAnimationLightMode;
  config.loopAnimationCount = -1;

  _aimAnimation = ios::provider::GenerateLottieAnimation(config);
  return _aimAnimation;
}

@end
