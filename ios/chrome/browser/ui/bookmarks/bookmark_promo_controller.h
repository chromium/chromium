// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;
@protocol SigninPresenter;
@class SigninPromoViewConfigurator;
@class SigninPromoViewMediator;

@protocol BookmarkPromoControllerDelegate

// Controls the state of the promo.
- (void)promoStateChanged:(BOOL)promoEnabled;

// Configures the sign-in promo view using `configurator`, and reloads the view
// needed.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

@end

// This controller manages the display of the promo cell through its delegate
// and handles displaying the sign-in view controller.
@interface BookmarkPromoController : NSObject

@property(nonatomic, weak) id<BookmarkPromoControllerDelegate> delegate;

// Holds the current state of the promo. When the promo state change, it will
// call the promoStateChanged: selector on the delegate.
@property(nonatomic) BOOL shouldShowSigninPromo;

@property(nonatomic, readonly) SigninPromoViewMediator* signinPromoViewMediator;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate
                           presenter:(id<SigninPresenter>)presenter;

// Called before the instance is deallocated.
- (void)shutdown;

// Hides the promo cell. It won't be presented again on this profile.
- (void)hidePromoCell;

// Updates `shouldShowSigninPromo` based on the sign-in state of the user.
- (void)updateShouldShowSigninPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMO_CONTROLLER_H_
