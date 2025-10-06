// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h"

#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The point size of the entry point's symbol.
const CGFloat kIconPointSize = 18.0;

// Constants for the new badge view.
const CGFloat kNewFeatureBadgeSize = 16.0;
const CGFloat kNewFeatureFontSize = 9;
const CGFloat kNewBadgeOffsetFromButtonCenterX = 14.0;
const CGFloat kNewBadgeOffsetFromButtonCenterY = 8.0;

// Constants for button shadow.
const CGFloat kButtonShadowOpacity = 0.05;
const CGFloat kButtonShadowRadius = 1.0;
const CGFloat kButtonShadowVerticalOffset = 3.0;

// The width of the extended button's tappable area.
const CGFloat kMinimumWidth = 44;

// The width of the background view.
const CGFloat kBackgroundWidth = 26;

// Scale factors for button.
const CGFloat kNormalScaling = 1.0;
const CGFloat kHighlightScaling = 0.7;

// Animation duration.
NSTimeInterval kAnimationDuration = 0.3;

}  // namespace

@implementation PageActionMenuEntrypointView {
  // Button's background subview.
  UIView* _backgroundView;
  // Whether the new badge is visible.
  BOOL _newBadgeVisible;
  // "New" badge in top-left corner.
  NewFeatureBadgeView* _newBadgeView;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _backgroundView = [[UIView alloc] init];
    self.pointerInteractionEnabled = YES;
    self.minimumDiameter = kMinimumWidth;
    self.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;

    if (IsDirectBWGEntryPoint()) {
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_BWG_ASK_GEMINI_ACCESSIBILITY_LABEL);
      self.accessibilityIdentifier =
          kGeminiDirectEntryPointAccessibilityIdentifier;
    } else {
      self.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_BWG_PAGE_ACTION_MENU_ENTRY_POINT_ACCESSIBILITY_LABEL);
      self.accessibilityIdentifier = kAIHubEntrypointAccessibilityIdentifier;
    }

    [self createBackgroundView];
    [self applyDefaultButtonState];

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintGreaterThanOrEqualToConstant:kMinimumWidth],
      [_backgroundView.widthAnchor constraintEqualToConstant:kBackgroundWidth],
      [_backgroundView.heightAnchor constraintEqualToConstant:kBackgroundWidth],
      [_backgroundView.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor],
      [_backgroundView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

- (void)setNewBadgeVisible:(BOOL)visible {
  if (_newBadgeVisible == visible) {
    return;
  }
  _newBadgeVisible = visible;

  if (_newBadgeVisible) {
    [self setEntrypointIconWithScale:kHighlightScaling];
    [self setUpButtonWithNewFeatureBadge];
  } else {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kAnimationDuration
                     animations:^{
                       [weakSelf removeNewFeatureBadge];
                       [weakSelf applyDefaultButtonState];
                     }];
  }
}

#pragma mark - PageActionMenuEntryPointCommands

- (void)toggleEntryPointHighlight:(BOOL)highlight {
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     [weakSelf updateHighlightState:highlight];
                   }];
}

#pragma mark - Private

// Updates properties related to highlighting the button.
- (void)updateHighlightState:(BOOL)shouldHighlight {
  if (shouldHighlight) {
    [self setEntrypointIconWithScale:kHighlightScaling];
    self.tintColor = [UIColor colorNamed:kSolidWhiteColor];
    _backgroundView.hidden = NO;
  } else {
    [self applyDefaultButtonState];
  }
}

// Creates button's background view.
- (void)createBackgroundView {
  _backgroundView.layer.cornerRadius = kBackgroundWidth / 2;
  _backgroundView.clipsToBounds = YES;
  _backgroundView.userInteractionEnabled = NO;
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [self insertSubview:_backgroundView atIndex:0];
}

// Creates a new badge and positions the badge relative to the button.
- (void)setUpButtonWithNewFeatureBadge {
  self.tintColor = [UIColor colorNamed:kBlue600Color];
  _backgroundView.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];
  _backgroundView.hidden = NO;
  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  self.layer.shadowOpacity = kButtonShadowOpacity;
  self.layer.shadowRadius = kButtonShadowRadius;

  _newBadgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewFeatureBadgeSize
                                            fontSize:kNewFeatureFontSize];
  _newBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  _newBadgeView.accessibilityElementsHidden = YES;
  [self addSubview:_newBadgeView];

  [NSLayoutConstraint activateConstraints:@[
    [_newBadgeView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor
                       constant:kNewBadgeOffsetFromButtonCenterX],
    [_newBadgeView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor
                       constant:-kNewBadgeOffsetFromButtonCenterY],
  ]];
}

// Removes the new feature badge.
- (void)removeNewFeatureBadge {
  if (_newBadgeView) {
    [_newBadgeView removeFromSuperview];
    _newBadgeView = nil;
  }
}

// Sets the button to default state.
- (void)applyDefaultButtonState {
  [self setEntrypointIconWithScale:kNormalScaling];
  self.tintColor = [UIColor colorNamed:kToolbarButtonColor];
  self.layer.shadowOpacity = 0;
  _backgroundView.hidden = YES;
  _backgroundView.backgroundColor = [UIColor colorNamed:kBlueColor];
}

// Sets the entry point icon with a scale factor.
- (void)setEntrypointIconWithScale:(CGFloat)scale {
  if (IsDirectBWGEntryPoint()) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    [self setImage:CustomSymbolWithPointSize(kGeminiBrandedLogoImage,
                                             kIconPointSize * scale)
          forState:UIControlStateNormal];
#else
    [self setImage:DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage,
                                              kIconPointSize * scale)
          forState:UIControlStateNormal];
#endif
  } else {
    [self setImage:CustomSymbolWithPointSize(kTextSparkSymbol,
                                             kIconPointSize * scale)
          forState:UIControlStateNormal];
  }
}

@end
