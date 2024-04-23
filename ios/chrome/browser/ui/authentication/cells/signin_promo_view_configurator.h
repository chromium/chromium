// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@class SigninPromoView;

// Class that configures a SigninPromoView instance.
@interface SigninPromoViewConfigurator : NSObject

// Initializes the instance.
// If `viewMode` is SigninPromoViewModeNoAccounts, then `userEmail`,
// `userGivenName` and `userImage` have to be nil.
// Otherwise `userEmail` and `userImage` can't be nil. `userImage` has to be to
// the size of IdentityAvatarSize::SmallSize.
// `hasSignInSpinner` shows a spinner on top of the primary button, and disables
// other buttons if set YES.
// TODO(crbug.com/40842280): Consider adding a parameter for
// SigninPromoViewStyle.
- (instancetype)initWithSigninPromoViewMode:(SigninPromoViewMode)viewMode
                                  userEmail:(NSString*)userEmail
                              userGivenName:(NSString*)userGivenName
                                  userImage:(UIImage*)userImage
                             hasCloseButton:(BOOL)hasCloseButton
                           hasSignInSpinner:(BOOL)hasSignInSpinner
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Overrides the default primary button title in signin promo. If unset/`nil`
// the default text is used.
@property(nonatomic, copy) NSString* primaryButtonTitleOverride;

// Configure `signinPromoView` with the given `promoViewStyle` style.
- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView
                       withStyle:(SigninPromoViewStyle)promoViewStyle;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_
