// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

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
    NSString* signInString =
        GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SIGNIN);
    signinPromoView.accessibilityLabel = signInString;
    [signinPromoView.primaryButton setTitle:signInString
                                   forState:UIControlStateNormal];
  } else {
    signinPromoView.mode = SigninPromoViewModeWarmState;
    NSString* name =
        self.userFullName.length ? self.userFullName : self.userEmail;
    base::string16 name16 = SysNSStringToUTF16(name);
    [signinPromoView.primaryButton
        setTitle:GetNSStringF(IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, name16)
        forState:UIControlStateNormal];
    signinPromoView.accessibilityLabel =
        GetNSStringF(IDS_IOS_SIGNIN_PROMO_ACCESSIBILITY_LABEL, name16);
    [signinPromoView.secondaryButton
        setTitle:GetNSString(IDS_IOS_SIGNIN_PROMO_CHANGE_ACCOUNT)
        forState:UIControlStateNormal];
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
