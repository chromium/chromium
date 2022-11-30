// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@implementation SigninPromoViewConfigurator

- (instancetype)initWithSigninPromoViewMode:(SigninPromoViewMode)viewMode
                                  userEmail:(NSString*)userEmail
                              userGivenName:(NSString*)userGivenName
                                  userImage:(UIImage*)userImage
                             hasCloseButton:(BOOL)hasCloseButton {
  self = [super init];
  if (self) {
    DCHECK(userEmail || (!userEmail && !userGivenName && !userImage));
    _signinPromoViewMode = viewMode;
    _userGivenName = [userGivenName copy];
    _userEmail = [userEmail copy];
    _userImage = [userImage copy];
    _hasCloseButton = hasCloseButton;
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
      // The profile icon should only appear for the standard signin promo view.
      // TODO(crbug.com/1331010): Adapt other styles to accept profile image
      // when we have UX approval.
      if (self.signinPromoViewMode != SigninPromoViewModeNoAccounts) {
        [self assignProfileImageToSigninPromoView:signinPromoView];
      }
      break;
    }
    case SigninPromoViewStyleTitled: {
      [self configureTitledPromoView:signinPromoView];
      break;
    }
    case SigninPromoViewStyleTitledCompact: {
      [self configureTitledPromoView:signinPromoView];
      break;
    }
  }
}

#pragma mark - Private

// Configures the view elements of the `signinPromoView` to conform to the
// `SigninPromoViewStyleStandard` style.
- (void)configureStandardSigninPromoView:(SigninPromoView*)signinPromoView {
  signinPromoView.titleLabel.hidden = YES;
  //  signinPromoView.secondaryButton.hidden = NO;
  NSString* name =
      self.userGivenName.length ? self.userGivenName : self.userEmail;
  std::u16string name16 = SysNSStringToUTF16(name);
  switch (self.signinPromoViewMode) {
    case SigninPromoViewModeNoAccounts: {
      DCHECK(!name);
      DCHECK(!self.userImage);
      NSString* signInString = GetNSString(IDS_IOS_SYNC_PROMO_TURN_ON_SYNC);
      [signinPromoView.primaryButton setTitle:signInString
                                     forState:UIControlStateNormal];
      break;
    }
    case SigninPromoViewModeSigninWithAccount: {
      [signinPromoView.primaryButton
          setTitle:GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, name16)
          forState:UIControlStateNormal];
      [signinPromoView.secondaryButton
          setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
          forState:UIControlStateNormal];
      break;
    }
    case SigninPromoViewModeSyncWithPrimaryAccount: {
      [signinPromoView.primaryButton
          setTitle:GetNSString(IDS_IOS_SYNC_PROMO_TURN_ON_SYNC)
          forState:UIControlStateNormal];
      break;
    }
  }
}

// Configures the view elements of the `signinPromoView` to conform to
// `SigninPromoViewStyleTitled` or `SigninPromoViewStyleTitledCompact` style.
- (void)configureTitledPromoView:(SigninPromoView*)signinPromoView {
  // In the titled Promo views (both compact and non compact the primary button
  // text will use "continue" regardless of the promo mode.
  signinPromoView.titleLabel.hidden = NO;
  //  signinPromoView.secondaryButton.hidden = YES;
  NSString* signInString = GetNSString(IDS_IOS_NTP_FEED_SIGNIN_PROMO_CONTINUE);
  [signinPromoView.primaryButton setTitle:signInString
                                 forState:UIControlStateNormal];
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

@end
