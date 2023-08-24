// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller_delegate.h"

class AuthenticationService;
@protocol FeedTopSectionConsumer;
@protocol NewTabPageDelegate;
class PrefService;
@class SigninPromoViewMediator;

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for the NTP Feed top section, handling the interactions.
@interface FeedTopSectionMediator
    : NSObject <FeedTopSectionViewControllerDelegate, SigninPromoViewConsumer>

- (instancetype)initWithConsumer:(id<FeedTopSectionConsumer>)consumer
                 identityManager:(signin::IdentityManager*)identityManager
                     authService:(AuthenticationService*)authService
                     isIncognito:(BOOL)isIncognito
                     prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The mediator handling the interactions of the signin promo.
@property(nonatomic, weak) SigninPromoViewMediator* signinPromoMediator;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> ntpDelegate;

// Returns `YES` if the signin promo exists on the current NTP.
@property(nonatomic, assign) BOOL isSignInPromoEnabled;

// Initializes the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
