// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Button content padding (Vertical and Horizontal).
const CGFloat kButtonPaddingV = 15.0f;
const CGFloat kButtonPaddingH = 38.0f;
// Max radius for the authenticate button background.
const CGFloat kAuthenticateButtonBagroundMaxCornerRadius = 30.0f;
// Distance from top and bottom to content (buttons/logos).
const CGFloat kVerticalContentPadding = 70.0f;
}  // namespace

@interface IncognitoReauthView () <IncognitoReauthViewLabelOwner>
// The background view for the authenticate button.
// Has to be separate from the button because it's a blur view (on iOS 13+).
@property(nonatomic, weak) UIView* authenticateButtonBackgroundView;
@end

@implementation IncognitoReauthView

- (instancetype)init {
  self = [super init];
  if (self) {
    // Increase blur intensity by layering some blur views to make
    // content behind really not recognizeable.
    for (int i = 0; i < 3; i++) {
      UIBlurEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
      UIVisualEffectView* blurView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      [self addSubview:blurView];
      blurView.translatesAutoresizingMaskIntoConstraints = NO;
      AddSameConstraints(self, blurView);
    }

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurBackgroundView];
    blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, blurBackgroundView);

    UIImage* incognitoLogo = CustomSymbolWithPointSize(kIncognitoSymbol, 28);
    _logoView = [[UIImageView alloc] initWithImage:incognitoLogo];
    _logoView.tintColor = UIColor.whiteColor;
    _logoView.translatesAutoresizingMaskIntoConstraints = NO;
    [blurBackgroundView.contentView addSubview:_logoView];
    AddSameCenterXConstraint(_logoView, blurBackgroundView);
    [_logoView.topAnchor
        constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide.topAnchor
                       constant:kVerticalContentPadding]
        .active = YES;

    _tabSwitcherButton = [[UIButton alloc] init];
    _tabSwitcherButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabSwitcherButton setTitleColor:[UIColor whiteColor]
                             forState:UIControlStateNormal];
    [_tabSwitcherButton setTitleColor:[UIColor colorWithWhite:1 alpha:0.4]
                             forState:UIControlStateHighlighted];

    [_tabSwitcherButton setTitle:l10n_util::GetNSString(
                                     IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
                        forState:UIControlStateNormal];
    _tabSwitcherButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    _tabSwitcherButton.titleLabel.adjustsFontSizeToFitWidth = YES;
    _tabSwitcherButton.titleLabel.adjustsFontForContentSizeCategory = YES;

    _tabSwitcherButton.pointerInteractionEnabled = YES;

    UIView* authButtonContainer =
        [self buildAuthenticateButtonWithBlurEffect:blurEffect];
    [blurBackgroundView.contentView addSubview:authButtonContainer];
    AddSameCenterConstraints(blurBackgroundView, authButtonContainer);
    _authenticateButtonBackgroundView = authButtonContainer;

    [blurBackgroundView.contentView addSubview:_tabSwitcherButton];
    AddSameCenterXConstraint(_tabSwitcherButton, blurBackgroundView);

    [NSLayoutConstraint activateConstraints:@[
      [_tabSwitcherButton.topAnchor
          constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                      .bottomAnchor
                         constant:-kVerticalContentPadding],
      [_authenticateButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
      [_tabSwitcherButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
    ]];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
      __weak IncognitoReauthView* weakSelf = self;
      [self registerForTraitChanges:traits
                        withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                      UITraitCollection* previousCollection) {
                          [weakSelf setNeedsLayout];
                          [weakSelf layoutIfNeeded];
                        }];
    }

    [self setNeedsLayout];
    [self layoutIfNeeded];
  }

  return self;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self setNeedsLayout];
  [self layoutIfNeeded];
}
#endif

// Creates _authenticateButton.
// Returns a "decoration" pill-shaped view containing _authenticateButton.
- (UIView*)buildAuthenticateButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  DCHECK(!_authenticateButton);

  // Use a IncognitoReauthViewLabel for the button label, because the built-in
  // UIButton's `titleLabel` does not correctly resize for multiline labels and
  // using a UILabel doesn't provide feedback to adjust the corner radius.
  IncognitoReauthViewLabel* titleLabel =
      [[IncognitoReauthViewLabel alloc] init];
  titleLabel.owner = self;
  titleLabel.numberOfLines = 0;
  titleLabel.textColor = [UIColor colorWithWhite:1 alpha:0.95];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  titleLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // Disable a11y; below the UIButton will get a correct label.
  titleLabel.accessibilityLabel = nil;

  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];

  button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;

  UIView* backgroundView = nil;
  UIVisualEffectView* effectView = [[UIVisualEffectView alloc]
      initWithEffect:[UIVibrancyEffect
                         effectForBlurEffect:blurEffect
                                       style:UIVibrancyEffectStyleFill]];

  [button addSubview:titleLabel];
  [effectView.contentView addSubview:button];
  backgroundView = effectView;
  AddSameConstraintsWithInsets(
      button, titleLabel,
      NSDirectionalEdgeInsetsMake(-kButtonPaddingV, -kButtonPaddingH,
                                  -kButtonPaddingV, -kButtonPaddingH));

  backgroundView.backgroundColor =
      [IncognitoReauthView blurButtonBackgroundColor];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(backgroundView, button);

  // Handle touch up and down events to create a "highlight" state.
  // The normal button highlight state is not usable here because the actual
  // button is transparent.
  [button addTarget:self
                action:@selector(blurButtonEventHandler)
      forControlEvents:UIControlEventAllEvents];

  _authenticateButton = button;

  return backgroundView;
}

#pragma mark - voiceover

- (BOOL)accessibilityViewIsModal {
  return YES;
}

- (BOOL)accessibilityPerformMagicTap {
  [self.authenticateButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  return YES;
}

#pragma mark - internal

- (void)blurButtonEventHandler {
  [UIView animateWithDuration:0.1
                   animations:^{
                     self.authenticateButtonBackgroundView.backgroundColor =
                         [self.authenticateButton isHighlighted]
                             ? [IncognitoReauthView
                                   blurButtonHighlightBackgroundColor]
                             : [IncognitoReauthView blurButtonBackgroundColor];
                   }];
}

#pragma mark - IncognitoReauthViewLabelOwner

- (void)labelDidLayout {
  CGFloat cornerRadius =
      std::min(kAuthenticateButtonBagroundMaxCornerRadius,
               self.authenticateButtonBackgroundView.frame.size.height / 2);
  self.authenticateButtonBackgroundView.layer.cornerRadius = cornerRadius;
}

#pragma mark - helpers

+ (UIColor*)blurButtonBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.15];
}

+ (UIColor*)blurButtonHighlightBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.6];
}

@end
