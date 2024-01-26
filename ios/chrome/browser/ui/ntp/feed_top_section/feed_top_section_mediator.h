// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mutator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view_constants.h"

class AuthenticationService;
@protocol NotificationsAlertPresenter;
@protocol NotificationsConfirmationPresenter;
@protocol FeedTopSectionConsumer;
@protocol NewTabPageDelegate;
class PrefService;
@class SigninPromoViewMediator;

namespace signin {
class IdentityManager;
}  // namespace signin

// Enum actions for content notification promo UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationTopOfFeedPromoAction enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationTopOfFeedPromoAction {
  kAccept = 0,
  kDecline = 1,
  kMainButtonTapped = 2,
  kDismissedFromCloseButton = 3,
  kDismissedFromSecondaryButton = 4,
  kMaxValue = kDismissedFromSecondaryButton,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// Enum events for content notification promo UMA metrics. Entries should not
// be renumbered and numeric values should never be reused. This should align
// with the ContentNotificationTopOfFeedPromoEvent enum in enums.xml.
//
// LINT.IfChange
enum class ContentNotificationTopOfFeedPromoEvent {
  kPromptShown = 0,
  kNotifActive = 1,
  kError = 2,
  kMaxValue = kError,
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

// Handler for displaying notification related alerts.
@property(nonatomic, weak) id<NotificationsAlertPresenter>
    notificationsPresenter;

// The presenter displays the notification confirmation message.
@property(nonatomic, weak) id<NotificationsConfirmationPresenter>
    messagePresenter;

// Initializes the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
