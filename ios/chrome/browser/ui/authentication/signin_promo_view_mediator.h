// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"

@class ChromeIdentity;
@protocol SigninPresenter;
@class SigninPromoViewConfigurator;
@protocol SigninPromoViewConsumer;

namespace signin_metrics {
enum class AccessPoint;
}

namespace ios {
class ChromeBrowserState;

// Enums for the sign-in promo view state. Those states are sequential, with no
// way to go backwards. All states can be skipped except |NeverVisible| and
// |Invalid|.
enum class SigninPromoViewState {
  // Initial state. When -[SigninPromoViewMediator
  // signinPromoViewIsRemoved] is called with that state, no metrics is
  // recorded.
  NeverVisible = 0,
  // None of the buttons has been used yet.
  Unused,
  // Sign-in buttons have been used at least once.
  UsedAtLeastOnce,
  // Sign-in promo has been closed.
  Closed,
  // Sign-in promo view has been removed.
  Invalid,
};
}  // namespace ios

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Class that monitors the available identities and creates
// SigninPromoViewConfigurator. This class makes the link between the model and
// the view. The consumer will receive notification if default identity is
// changed or updated.
@interface SigninPromoViewMediator : NSObject<SigninPromoViewDelegate>

// Consumer to handle identity update notifications.
@property(nonatomic, weak) id<SigninPromoViewConsumer> consumer;

// Chrome identity used to configure the view in a warm state mode. Otherwise
// contains nil.
@property(nonatomic, strong, readonly) ChromeIdentity* defaultIdentity;

// Sign-in promo view state.
@property(nonatomic, assign) ios::SigninPromoViewState signinPromoViewState;

// YES if the sign-in interaction controller is shown.
@property(nonatomic, assign, readonly, getter=isSigninInProgress)
    BOOL signinInProgress;

// Returns YES if the sign-in promo view is |Invalid|, |Closed| or invisible.
@property(nonatomic, assign, readonly, getter=isInvalidClosedOrNeverVisible)
    BOOL invalidClosedOrNeverVisible;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Tests if the sign-in promo view should be displayed according to the number
// of times it has been displayed and if the user closed the sign-in promo view.
+ (BOOL)shouldDisplaySigninPromoViewWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                       browserState:(ios::ChromeBrowserState*)
                                                        browserState;

// See -[SigninPromoViewMediator initWithBrowserState:].
- (instancetype)init NS_UNAVAILABLE;

// Initialises with browser state.
// * |browserState| is the current browser state.
// * |accessPoint| only ACCESS_POINT_SETTINGS, ACCESS_POINT_BOOKMARK_MANAGER,
// ACCESS_POINT_RECENT_TABS, ACCESS_POINT_TAB_SWITCHER are supported.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                           presenter:(id<SigninPresenter>)presenter
    NS_DESIGNATED_INITIALIZER;

- (SigninPromoViewConfigurator*)createConfigurator;

// Increments the "shown" counter used for histograms. Called when the signin
// promo view is visible. If the sign-in promo is already visible, this method
// does nothing.
- (void)signinPromoViewIsVisible;

// Called when the sign-in promo view is hidden. If the sign-in promo view has
// never been shown, or it is already hidden, this method does nothing.
- (void)signinPromoViewIsHidden;

// Called when the sign-in promo view is removed from the view hierarchy (it or
// one of its superviews is removed). The mediator should not be used after this
// called.
- (void)signinPromoViewIsRemoved;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
