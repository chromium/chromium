// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninPromoViewConfigurator ()

// User email used for the secondary button, and also for the primary button if
// there is no userFullName.
@property(nonatomic) NSString* userEmail;

// User full name used fro the primary button.
@property(nonatomic) NSString* userFullName;

// User profile image.
@property(nonatomic) UIImage* userImage;

// If YES the close button will be shown.
@property(nonatomic) BOOL hasCloseButton;

@end

@implementation SigninPromoViewConfigurator

@synthesize userEmail = _userEmail;
@synthesize userFullName = _userFullName;
@synthesize userImage = _userImage;
@synthesize hasCloseButton = _hasCloseButton;

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     userFullName:(NSString*)userFullName
                        userImage:(UIImage*)userImage
                   hasCloseButton:(BOOL)hasCloseButton {
  self = [super init];
  if (self) {
    DCHECK(userEmail || (!userEmail && !userFullName && !userImage));
    _userFullName = [userFullName copy];
    _userEmail = [userEmail copy];
    _userImage = [userImage copy];
    _hasCloseButton = hasCloseButton;
  }
  return self;
}

- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView {
  signinPromoView.closeButton.hidden = !self.hasCloseButton;
  if (!self.userEmail) {
    signinPromoView.mode = SigninPromoViewModeColdState;
  } else {
    signinPromoView.mode = SigninPromoViewModeWarmState;
    NSString* name =
        self.userFullName.length ? self.userFullName : self.userEmail;
    [signinPromoView.primaryButton
        setTitle:l10n_util::GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS,
                                         base::SysNSStringToUTF16(name))
        forState:UIControlStateNormal];
    if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
      [signinPromoView.secondaryButton
          setTitle:l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
          forState:UIControlStateNormal];
    } else {
      [signinPromoView.secondaryButton
          setTitle:l10n_util::GetNSStringF(
                       IDS_IOS_SIGNIN_PROMO_NOT,
                       base::SysNSStringToUTF16(self.userEmail))
          forState:UIControlStateNormal];
    }
    UIImage* image = self.userImage;
    if (!image) {
      image = ios::GetChromeBrowserProvider()
                  ->GetSigninResourcesProvider()
                  ->GetDefaultAvatar();
    }
    [signinPromoView setProfileImage:image];
  }
}

@end
