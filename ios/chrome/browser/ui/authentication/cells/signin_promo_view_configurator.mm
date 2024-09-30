// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/l10n/l10n_util.h"

using base::SysNSStringToUTF16;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@interface SigninPromoViewConfigurator ()

// User email used for the secondary button, and also for the primary button if
// there is no userGivenName.
@property(nonatomic) NSString* userEmail;

// User full name used fro the primary button.
@property(nonatomic) NSString* userGivenName;

// User profile image.
@property(nonatomic) UIImage* userImage;

// If YES the close button will be shown.
@property(nonatomic) BOOL hasCloseButton;

// State of the identity promo view.
@property(nonatomic, assign) SigninPromoViewMode signinPromoViewMode;

@end

@implementation SigninPromoViewConfigurator {
  // Shows a spinner on top of the primary button, and disables ther buttons if
  // set to YES.
  BOOL _hasSignInSpinner;
}

- (instancetype)initWithSigninPromoViewMode:(SigninPromoViewMode)viewMode
                                  userEmail:(NSString*)userEmail
                              userGivenName:(NSString*)userGivenName
                                  userImage:(UIImage*)userImage
                             hasCloseButton:(BOOL)hasCloseButton
                           hasSignInSpinner:(BOOL)hasSignInSpinner {
  self = [super init];
  if (self) {
    DCHECK(userEmail || (!userEmail && !userGivenName && !userImage));
    _signinPromoViewMode = viewMode;
    _userGivenName = [userGivenName copy];
    _userEmail = [userEmail copy];
    _userImage = [userImage copy];
    _hasCloseButton = hasCloseButton;
    _hasSignInSpinner = hasSignInSpinner;
  }
  return self;
}

- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView
                       withStyle:(SigninPromoViewStyle)promoViewStyle {
  signinPromoView.closeButton.hidden = !self.hasCloseButton;
  signinPromoView.mode = self.signinPromoViewMode;
  signinPromoView.promoViewStyle = promoViewStyle;
  switch (promoViewStyle) {
    case SigninPromoViewStyleStandard: {
      [self configureStandardSigninPromoView:signinPromoView];
      break;
    }
    case SigninPromoViewStyleCompact: {
      [self configureCompactPromoView:signinPromoView withStyle:promoViewStyle];
      break;
    }
    case SigninPromoViewStyleOnlyButton:
      [self configureOnlyButtonPromoView:signinPromoView];
      break;
  }
  if (_hasSignInSpinner) {
    [signinPromoView startSignInSpinner];
  } else {
    [signinPromoView stopSignInSpinner];
  }
}

#pragma mark - Private

// Configures the view elements of the `signinPromoView` to conform to the
// `SigninPromoViewStyleStandard` style.
- (void)configureStandardSigninPromoView:(SigninPromoView*)signinPromoView {
  NSString* name =
      self.userGivenName.length ? self.userGivenName : self.userEmail;
  std::u16string name16 = SysNSStringToUTF16(name);
  switch (self.signinPromoViewMode) {
    case SigninPromoViewModeNoAccounts: {
      DCHECK(!name);
      DCHECK(!self.userImage);
      NSString* signInString =
          self.primaryButtonTitleOverride
              ? self.primaryButtonTitleOverride
              : GetNSString(IDS_IOS_SYNC_PROMO_TURN_ON_SYNC);
      [signinPromoView configurePrimaryButtonWithTitle:signInString];
      break;
    }
    case SigninPromoViewModeSigninWithAccount: {
      [signinPromoView
          configurePrimaryButtonWithTitle:GetNSStringF(
                                              IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                              name16)];
      [signinPromoView.secondaryButton
          setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
          forState:UIControlStateNormal];
      [self assignProfileImageToSigninPromoView:signinPromoView];
      break;
    }
    case SigninPromoViewModeSignedInWithPrimaryAccount: {
      [signinPromoView configurePrimaryButtonWithTitle:
                           self.primaryButtonTitleOverride
                               ? self.primaryButtonTitleOverride
                               : GetNSString(IDS_IOS_SYNC_PROMO_TURN_ON_SYNC)];
      [self assignProfileImageToSigninPromoView:signinPromoView];
      break;
    }
  }
}

// Configures the view elements of the `signinPromoView` to conform to a compact
// style.
- (void)configureCompactPromoView:(SigninPromoView*)signinPromoView
                        withStyle:(SigninPromoViewStyle)promoStyle {
  switch (promoStyle) {
    case SigninPromoViewStyleStandard:
    case SigninPromoViewStyleOnlyButton:
      // This function shouldn't be used for the non-compact promos.
      NOTREACHED();
    case SigninPromoViewStyleCompact:
      [signinPromoView configurePrimaryButtonWithTitle:
                           GetNSString(IDS_IOS_NTP_FEED_SIGNIN_PROMO_CONTINUE)];
      switch (self.signinPromoViewMode) {
        case SigninPromoViewModeNoAccounts:
          DCHECK(!self.userImage);
          [self assignNonProfileImageToSigninPromoView:signinPromoView];
          break;
        case SigninPromoViewModeSigninWithAccount:
        case SigninPromoViewModeSignedInWithPrimaryAccount:
          [self assignProfileImageToSigninPromoView:signinPromoView];
          break;
      }
  }
}

// Configures the view elements of the `signinPromoView` to conform to the
// `SigninPromoViewStyleOnlyButton` style.
- (void)configureOnlyButtonPromoView:(SigninPromoView*)signinPromoView {
  [signinPromoView
      configurePrimaryButtonWithTitle:l10n_util::GetNSString(
                                          IDS_IOS_SIGNIN_PROMO_TURN_ON)];
}

// Sets profile image to a given `signinPromoView`.
- (void)assignProfileImageToSigninPromoView:(SigninPromoView*)signinPromoView {
  UIImage* image = self.userImage;
  DCHECK(image);
  CGSize avatarSize =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::SmallSize);
  DCHECK_EQ(avatarSize.width, image.size.width);
  DCHECK_EQ(avatarSize.height, image.size.height);
  [signinPromoView setProfileImage:image];
}

// Sets non-profile image to a given `signinPromoView`.
- (void)assignNonProfileImageToSigninPromoView:
    (SigninPromoView*)signinPromoView {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logo = [UIImage imageNamed:kChromeSigninPromoLogoImage];
#else
  UIImage* logo = [UIImage imageNamed:kChromiumSigninPromoLogoImage];
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  DCHECK(logo);
  [signinPromoView setNonProfileImage:logo];
}

@end
