// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_trailing_button.h"

#import "base/check.h"
#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {
/// Size of the trailing button.
const CGFloat kTrailingButtonIconPointSize = 17.0f;

/// Size of the aim button icon.
const CGFloat kAimButtonIconPointSize = 18.0f;

/// The animation view size.
const CGSize kAimAnimationViewSize = {40.0f, 40.0f};

// The name of the animation for the AIM button.
NSString* const kAIMCircleAnimationLightMode = @"mia_circle_animation_no_glow";
NSString* const kAIMCircleAnimationDarkMode = @"mia_glowing_circle_animation";

}  // namespace

@implementation OmniboxPopupRowTrailingButton {
  /// The aim animation view.
  UIView* _aimAnimationView;
  /// The aim lottie animation.
  id<LottieAnimation> _aimAnimation;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveMemoryWarning)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
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

  self.hidden = NO;
  UIImage* icon;

  switch (trailingIconType) {
    case TrailingIconType::kNone:
      self.accessibilityIdentifier = nil;
      self.hidden = YES;
      return;
    case TrailingIconType::kSearchWithAim:
      icon = MakeSymbolMonochrome(CustomSymbolWithPointSize(
          kMagnifyingglassSparkSymbol, kAimButtonIconPointSize));
      self.accessibilityIdentifier =
          kOmniboxPopupRowSearchWithAimAccessibilityIdentifier;
      [self setupSearchWithAimAnimationView];
      break;
    case TrailingIconType::kRefineQuery:
      icon = DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                        kTrailingButtonIconPointSize);
      self.accessibilityIdentifier =
          kOmniboxPopupRowAppendAccessibilityIdentifier;
      break;
    case TrailingIconType::kOpenExistingTab:
      icon = DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                        kTrailingButtonIconPointSize);
      self.accessibilityIdentifier =
          kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
      break;
  }

  if (trailingIconType != TrailingIconType::kSearchWithAim) {
    // `imageWithHorizontallyFlippedOrientation` is flipping the icon
    // automatically when the UI is RTL/LTR.
    icon = [icon imageWithHorizontallyFlippedOrientation];
    icon = [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  [self setImage:icon forState:UIControlStateNormal];
  [self updateTintColor];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (_isHighlighted == isHighlighted) {
    return;
  }

  _isHighlighted = isHighlighted;
  [self updateTintColor];
}

#pragma mark - Low memory warning

- (void)didReceiveMemoryWarning {
  // If the animation view is not on screen, we can safely purge it.
  if (!_aimAnimationView.window) {
    _aimAnimation = nil;
    _aimAnimationView = nil;
  }
}

#pragma mark - private

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

// Setups the search with Aim animation.
- (void)setupSearchWithAimAnimationView {
  self.pointerInteractionEnabled = YES;
  self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();

  [self addSubview:self.aimAnimationView];
  self.aimAnimationView.userInteractionEnabled = NO;
  AddSameCenterConstraints(self.aimAnimationView, self);
  AddSizeConstraints(self.aimAnimationView, kAimAnimationViewSize);
  [self.aimLottieAnimation play];
}

// Removes the aim animation for the view.
- (void)removeSearchWithAimAnimationViewIfNeeded {
  if (self.trailingIconType == TrailingIconType::kSearchWithAim) {
    return;
  }

  [self.aimAnimationView removeFromSuperview];
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
