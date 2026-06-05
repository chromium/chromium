// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/ntp_identity_disc_button.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The maximum point size of the font for the Identity Disc button.
const CGFloat kIdentityDiscMaxFontSize = 24;

// Horizontal padding between the edge of the pill and its label.
const CGFloat kPillHorizontalPadding = 13;

// Vertical padding between the edge of the pill and its label.
const CGFloat kPillVerticalPadding = 11;

// Multiplier for applying margins on multiple sides
const CGFloat kMarginMultiplier = 2;

// The offset of the account error badge from the ADP center.
constexpr CGFloat kAccountBadgeOffsetFromDiscCenter = 10.0;

// The size of the account error badge that is on top the ADP.
constexpr CGFloat kErrorSymbolPointSize = 16.0;

UIColor* AccountParticleDiscBadgeBackgroundColor(UIUserInterfaceStyle style) {
  if (style == UIUserInterfaceStyleDark) {
    return [UIColor colorNamed:kBackgroundColor];
  } else {
    return [UIColor colorNamed:@"ntp_background_color"];
  }
}

}  // namespace

@implementation NTPIdentityDiscButton {
  UIImageView* _accountDiscParticleBadgeImageView;
  BOOL _isSignedIn;
  BOOL _hasAccountError;
  UIImage* _identityDiscImage;
  NSString* _identityDiscAccessibilityLabel;
  NSLayoutConstraint* _widthConstraint;
  NSLayoutConstraint* _heightConstraint;
  NSLayoutConstraint* _trailingConstraint;
  NSLayoutConstraint* _capsuleWidthConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.accessibilityIdentifier = kNTPFeedHeaderIdentityDisc;
    self.pointerInteractionEnabled = YES;

    __weak __typeof(self) weakSelf = self;
    self.pointerStyleProvider = ^UIPointerStyle*(
        UIButton* button, UIPointerEffect* proposedEffect,
        UIPointerShape* proposedShape) {
      CGFloat singleInset =
          (button.frame.size.width - ntp_home::kIdentityAvatarDimension) / 2;
      CGRect rect = CGRectInset(button.frame, singleInset, singleInset);
      UIPointerShape* shape =
          [UIPointerShape shapeWithRoundedRect:rect
                                  cornerRadius:rect.size.width / 2];
      return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
    };

    // Initialize with signed out state by default.
    [self setSignedOutAccountImage];

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                      withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                    UITraitCollection* previousCollection) {
                        [weakSelf updateBadgeBackgroundColor];
                      }];
  }
  return self;
}

- (void)setupConstraintsWithTrailingAnchor:
    (NSLayoutXAxisAnchor*)trailingAnchor {
  _widthConstraint = [self.widthAnchor constraintEqualToConstant:0];
  _heightConstraint = [self.heightAnchor constraintEqualToConstant:0];
  _trailingConstraint =
      [self.trailingAnchor constraintEqualToAnchor:trailingAnchor constant:0];
  _trailingConstraint.active = YES;
  _capsuleWidthConstraint =
      [self.widthAnchor constraintGreaterThanOrEqualToAnchor:self.heightAnchor
                                                  multiplier:2.0];

  [self updateIdentityDiscConstraints];
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)setSignedOutAccountImage {
  _identityDiscImage = DefaultSymbolTemplateWithPointSize(
      kPersonCropCircleSymbol, ntp_home::kSignedOutIdentityIconSize);

  _isSignedIn = NO;

  _identityDiscAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_BUTTON_ACCESSIBILITY_LABEL);

  [self updateIdentityDiscState];
  [self updateIdentityDiscConstraints];
}

- (void)updateAccountImage:(UIImage*)image
                      name:(NSString*)name
                     email:(NSString*)email {
  DCHECK(image && image.size.width == ntp_home::kIdentityAvatarDimension &&
         image.size.height == ntp_home::kIdentityAvatarDimension)
      << base::SysNSStringToUTF8([image description]);
  DCHECK(email);

  _identityDiscImage = image;

  _isSignedIn = YES;

  [self updateIdentityDiscAccessibilityLabelWithName:name email:email];
  [self updateIdentityDiscConstraints];
}

#pragma mark - Public

- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email {
  if (_hasAccountError == hasAccountError) {
    return;
  }
  _hasAccountError = hasAccountError;
  if (_hasAccountError) {
    [self setIdentityDiscErrorBadge];
  } else {
    [self removeIdentityDiscErrorBadge];
  }

  if (email.length > 0) {
    [self updateIdentityDiscAccessibilityLabelWithName:name email:email];
  }
}

- (void)updateConfigurationWithPalette:(NewTabPageColorPalette*)colorPalette {
  if (_isSignedIn) {
    [self updateIdentityDiscState];
    return;
  }

  [self updateIdentityDiscStateWithPalette:colorPalette];
}

- (void)updateIdentityDiscConstraints {
  BOOL showSignInButtonWithoutAvatar = !_isSignedIn;

  CGFloat dimension = ntp_home::kIdentityAvatarDimension +
                      kMarginMultiplier * ntp_home::kHeaderIconMargin;

  CGFloat identityAvatarPadding = ntp_home::kIdentityAvatarPadding;

  if (showSignInButtonWithoutAvatar) {
    identityAvatarPadding *= kMarginMultiplier;
  } else {
    dimension += ntp_home::kHeaderIconMargin;
    identityAvatarPadding -= ntp_home::kHeaderIconMargin / 2;
  }

  _widthConstraint.constant = dimension;
  _heightConstraint.constant = dimension;
  if (showSignInButtonWithoutAvatar) {
    _widthConstraint.active = NO;
    _heightConstraint.active = NO;
    _capsuleWidthConstraint.active = YES;
  } else {
    _capsuleWidthConstraint.active = NO;
    _widthConstraint.active = YES;
    _heightConstraint.active = YES;
  }
  _trailingConstraint.constant = -identityAvatarPadding;
}

#pragma mark - Private

// Configures the button with the current state of `identityDiscImage`.
- (void)updateIdentityDiscState {
  DCHECK(_identityDiscImage);
  DCHECK(_identityDiscAccessibilityLabel);

  self.accessibilityLabel = _identityDiscAccessibilityLabel;
  self.clipsToBounds = YES;

  if (_isSignedIn) {
    UIImage* image = _identityDiscImage;
    self.configuration = nil;
    [self setImage:image forState:UIControlStateNormal];
    self.backgroundColor = nil;
    self.imageView.layer.cornerRadius = image.size.width / 2;
    self.imageView.layer.masksToBounds = YES;
    self.layer.cornerRadius = image.size.width;
    return;
  }

  // Signed out state styling.
  [self updateIdentityDiscStateWithPalette:nil];
}

// Configures the button for the signed-out state, applying the given color
// palette or falling back to the trait collection's palette.
- (void)updateIdentityDiscStateWithPalette:
    (NewTabPageColorPalette*)colorPalette {
  self.layer.cornerRadius = 0;
  [self setImage:nil forState:UIControlStateNormal];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  UIColor* foregroundColor = nil;
  UIColor* backgroundColor = nil;

  NewTabPageColorPalette* palette = colorPalette;
  if (!palette) {
    palette = [self.traitCollection objectForNewTabPageTrait];
  }

  if (!IsNTPBackgroundCustomizationEnabled()) {
    backgroundColor = [self defaultButtonBackgroundColor];
    foregroundColor = [UIColor colorNamed:kBlue600Color];
  } else {
    if ([self.traitCollection boolForNewTabPageImageBackgroundTrait]) {
      UIVisualEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
      UIVisualEffectView* blurBackgroundView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      buttonConfiguration.background.customView = blurBackgroundView;

      foregroundColor = [UIColor colorNamed:kTextPrimaryColor];
    } else {
      foregroundColor =
          palette ? palette.tintColor : [UIColor colorNamed:kBlue600Color];
      backgroundColor = palette ? palette.headerButtonColor
                                : [self defaultButtonBackgroundColor];
    }
  }

  if (backgroundColor) {
    buttonConfiguration.background.backgroundColor = backgroundColor;
  }

  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(kPillVerticalPadding, kPillHorizontalPadding,
                                  kPillVerticalPadding, kPillHorizontalPadding);

  NSDictionary* attributes = @{
    NSFontAttributeName : PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                                    UIFontWeightSemibold,
                                                    kIdentityDiscMaxFontSize),
    NSForegroundColorAttributeName : foregroundColor,
  };
  buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(IDS_IOS_SIGNIN_BUTTON_TEXT)
          attributes:attributes];

  self.configuration = buttonConfiguration;

  [self updateBadgeBackgroundColor];
}

// Creates and adds the error badge image view to the button with appropriate
// constraints.
- (void)setIdentityDiscErrorBadge {
  _accountDiscParticleBadgeImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kErrorCircleFillSymbol,
                                               kErrorSymbolPointSize)];
  _accountDiscParticleBadgeImageView.translatesAutoresizingMaskIntoConstraints =
      NO;
  _accountDiscParticleBadgeImageView.tintColor =
      [UIColor colorNamed:kRed500Color];
  [self updateBadgeBackgroundColor];
  _accountDiscParticleBadgeImageView.layer.cornerRadius =
      _accountDiscParticleBadgeImageView.frame.size.width / 2;
  _accountDiscParticleBadgeImageView.clipsToBounds = YES;
  _accountDiscParticleBadgeImageView.accessibilityIdentifier =
      kNTPFeedHeaderIdentityDiscBadge;

  [self addSubview:_accountDiscParticleBadgeImageView];

  [NSLayoutConstraint activateConstraints:@[
    [_accountDiscParticleBadgeImageView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor
                       constant:kAccountBadgeOffsetFromDiscCenter],
    [_accountDiscParticleBadgeImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor
                       constant:kAccountBadgeOffsetFromDiscCenter],
  ]];
}

// Removes the error badge from the button.
- (void)removeIdentityDiscErrorBadge {
  [_accountDiscParticleBadgeImageView removeFromSuperview];
  _accountDiscParticleBadgeImageView = nil;
}

// Generates and updates the accessibility label based on the user's name,
// email, and error state.
- (void)updateIdentityDiscAccessibilityLabelWithName:(NSString*)name
                                               email:(NSString*)email {
  NSString* accountButtonLabel;
  if (name) {
    accountButtonLabel =
        _hasAccountError
            ? l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU_WITH_ERROR,
                  base::SysNSStringToUTF16(name),
                  base::SysNSStringToUTF16(email))
            : l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU,
                  base::SysNSStringToUTF16(name),
                  base::SysNSStringToUTF16(email));
  } else {
    accountButtonLabel =
        _hasAccountError
            ? l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_EMAIL_OPEN_ACCOUNT_MENU_WITH_ERROR,
                  base::SysNSStringToUTF16(email))
            : l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_EMAIL_OPEN_ACCOUNT_MENU,
                  base::SysNSStringToUTF16(email));
  }

  _identityDiscAccessibilityLabel = accountButtonLabel;

  [self updateIdentityDiscState];
}

// Returns the default background color for the sign-in button based on the
// user interface style.
- (UIColor*)defaultButtonBackgroundColor {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                   ? [UIColor colorNamed:kTabGroupFaviconBackgroundColor]
                   : [[UIColor colorNamed:kSolidWhiteColor]
                         colorWithAlphaComponent:0.75];
      }];
}

// Updates the background color of the error badge to match the current user
// interface style.
- (void)updateBadgeBackgroundColor {
  if (_accountDiscParticleBadgeImageView) {
    _accountDiscParticleBadgeImageView.backgroundColor =
        AccountParticleDiscBadgeBackgroundColor(
            self.traitCollection.userInterfaceStyle);
  }
}

@end
