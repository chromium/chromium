// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_view_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"

class AuthenticationService;
@protocol NotificationsAlertPresenter;
@protocol FeedTopSectionConsumer;
@protocol NewTabPageDelegate;
class PrefService;
@class SigninPromoViewMediator;

namespace signin {
class IdentityManager;
}  // namespace signin

// Enum Provisional notifications entrypoint for UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationTopOfFeedPromoEvent enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationPromoProvisionalEntrypoint {
  kCloseButton = 0,
  kShownThreshold = 1,
  kMaxValue = kShownThreshold,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Mediator for the NTP Feed top section, handling the interactions.
@interface FeedTopSectionMediator
    : NSObject <FeedTopSectionMutator,
                FeedTopSectionViewControllerDelegate,
                SigninPromoViewConsumer>

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
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// Returns `YES` if the signin promo exists on the current NTP.
@property(nonatomic, assign) BOOL isSignInPromoEnabled;

// Returns `YES` if the user is using Google as default search engine.
@property(nonatomic, assign) BOOL isDefaultSearchEngine;

// Handler for displaying notification related alerts.
@property(nonatomic, weak) id<NotificationsAlertPresenter> presenter;

// Initializes the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

// Used from the coordinator to respond to the OS prompt outcome.
- (void)updateFeedTopSectionWhenClosed;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
