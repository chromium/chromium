// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARK_PROMO_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARK_PROMO_CONTROLLER_H_

#import <UIKit/UIKit.h>

class Browser;
@protocol SigninPresenter;
@protocol AccountSettingsPresenter;
@class SigninPromoViewConfigurator;
@class SigninPromoViewMediator;

namespace syncer {
class SyncService;
}

@protocol BookmarkPromoControllerDelegate

// Controls the state of the promo.
- (void)promoStateChanged:(BOOL)promoEnabled;

// Configures the sign-in promo view using `configurator`, and reloads the view
// needed.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

// Returns YES if the initial sync is running.
- (BOOL)isPerformingInitialSync;

@end

// This controller manages the display of the promo cell through its delegate
// and handles displaying the sign-in view controller.
@interface BookmarkPromoController : NSObject

@property(nonatomic, weak) id<BookmarkPromoControllerDelegate> delegate;

// Holds the current state of the promo. When the promo state change, it will
// call the promoStateChanged: selector on the delegate.
@property(nonatomic) BOOL shouldShowSigninPromo;

@property(nonatomic, readonly) SigninPromoViewMediator* signinPromoViewMediator;

// See `-[BookmarkPromoController initWithBrowser:delegate:presenter:
// baseViewController:]`.
- (instancetype)init NS_UNAVAILABLE;
// Designated initializer.
// `baseViewController` is the view to present UI for sign-in.
- (instancetype)initWithBrowser:(Browser*)browser
                    syncService:(syncer::SyncService*)syncService
                       delegate:(id<BookmarkPromoControllerDelegate>)delegate
                signinPresenter:(id<SigninPresenter>)signinPresenter
       accountSettingsPresenter:
           (id<AccountSettingsPresenter>)accountSettingsPresenter
    NS_DESIGNATED_INITIALIZER;

// Called before the instance is deallocated.
- (void)shutdown;

// Hides the promo cell. It won't be presented again on this profile.
- (void)hidePromoCell;

// Updates `shouldShowSigninPromo` based on the sign-in state of the user.
- (void)updateShouldShowSigninPromo;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARK_PROMO_CONTROLLER_H_
