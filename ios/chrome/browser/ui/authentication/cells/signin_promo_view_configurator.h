// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@class SigninPromoView;

// Class that configures a SigninPromoView instance.
@interface SigninPromoViewConfigurator : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Initializes the instance.
// |userEmail| is used as a proxy to determine if accounts have
// been added to the device.
- (instancetype)initWithIdentityPromoViewMode:(IdentityPromoViewMode)viewMode
                                    userEmail:(NSString*)userEmail
                                userGivenName:(NSString*)userGivenName
                                    userImage:(UIImage*)userImage
                               hasCloseButton:(BOOL)hasCloseButton
    NS_DESIGNATED_INITIALIZER;

// Configures a sign-in promo view.
- (void)configureSigninPromoView:(SigninPromoView*)signinPromoView;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_CONFIGURATOR_H_
