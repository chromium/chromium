// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mediator.h"

#import <UserNotifications/UserNotifications.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@interface FeedTopSectionMediator () <IdentityManagerObserverBridgeDelegate> {
  // Observes changes in identity.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
}

@property(nonatomic, assign) AuthenticationService* authenticationService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) BOOL isIncognito;
@property(nonatomic, assign) PrefService* prefService;

// Consumer for this mediator.
@property(nonatomic, weak) id<FeedTopSectionConsumer> consumer;

@end

@implementation FeedTopSectionMediator

// FeedTopSectionViewControllerDelegate
@synthesize signinPromoConfigurator = _signinPromoConfigurator;

- (instancetype)initWithConsumer:(id<FeedTopSectionConsumer>)consumer
                 identityManager:(signin::IdentityManager*)identityManager
                     authService:(AuthenticationService*)authenticationService
                     isIncognito:(BOOL)isIncognito
                     prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(_identityManager, self));
    _isIncognito = isIncognito;
    _prefService = prefService;
    _consumer = consumer;
  }
  return self;
}

- (void)setUp {
  [self updateShouldShowPromo];
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  _identityObserverBridge.reset();
  self.authenticationService = nullptr;
  self.identityManager = nullptr;
  self.prefService = nullptr;
}

// Handles closing the promo, and the NTP and Feed Top Section layout when the
// promo is closed.
- (void)updateFeedTopSectionWhenClosed {
  [self.NTPDelegate handleFeedTopSectionClosed];
  [self.consumer hidePromo];
  [self.NTPDelegate updateFeedLayout];
}

#pragma mark - FeedTopSectionViewControllerDelegate

- (SigninPromoViewConfigurator*)signinPromoConfigurator {
  if (!_signinPromoConfigurator) {
    _signinPromoConfigurator = [_signinPromoMediator createConfigurator];
  }
  return _signinPromoConfigurator;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!self.signinPromoMediator.showSpinner) {
        // User has signed in, stop showing the promo.
        [self updateShouldShowPromo];
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateShouldShowPromo];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  // No-op: The NTP is always recreated when the identity changes, so this is
  // not needed.
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self updateFeedTopSectionWhenClosed];
}

#pragma mark - FeedTopSectionMutator

- (void)notificationsPromoViewDismissedFromButton:
    (NotificationsPromoButtonType)buttonType {
  [self updateFeedTopSectionWhenClosed];
  // Update prefs that save the dismissed times if the promo conditions are not
  // being overriden.
  int notificationsPromoTimesDismissed =
      self.prefService->GetInteger(prefs::kNotificationsPromoTimesDismissed);
  if (!experimental_flags::ShouldForceContentNotificationsPromo()) {
    self.prefService->SetTime(prefs::kNotificationsPromoLastDismissed,
                              base::Time::Now());
    self.prefService->SetInteger(prefs::kNotificationsPromoTimesDismissed,
                                 notificationsPromoTimesDismissed + 1);
  }
  switch (buttonType) {
    case NotificationsPromoButtonTypeClose:
      [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                      kDismissedFromCloseButton];
      if (notificationsPromoTimesDismissed >=
          kNotificationsPromoMaxDismissedCount) {
        [self enrollUserToProvisionalNotificationsFromEntrypoint:
                  ContentNotificationPromoProvisionalEntrypoint::kCloseButton];
      }
      break;
    case NotificationsPromoButtonTypeSecondary:
      // If notification is dismissed from secondary button, set TimesDismissed
      // > kNotificationsPromoMaxDismissedCount, to ensure the user doesn't see
      // the notifications promo anymore.
      self.prefService->SetInteger(prefs::kNotificationsPromoTimesDismissed,
                                   kMaxImpressionsForDismissedThreshold);
      [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                      kDismissedFromSecondaryButton];
      break;
    case NotificationsPromoButtonTypePrimary:
      // This should never be executed as the primary button does not close the
      // promo.
      DCHECK(false);
      break;
  }
}

- (void)notificationsPromoViewMainButtonWasTapped {
  // Show the Notifications promo alert.
  RecordAction(UserMetricsAction(
      "ContentNotifications.Promo.TopOfFeed.MainButtonTapped"));
  [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                  kMainButtonTapped];
  [self.presenter presentPushNotificationPermissionAlert];
}

#pragma mark - Private

- (BOOL)isUserSignedIn {
  return self.identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

// Returns true if notifications are enabled in Chime or at the OS level.
- (BOOL)isNotificationsEnabled {
  DCHECK([self isUserSignedIn]);
  id<SystemIdentity> identity = self.authenticationService->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
  // Check if user has notifications enabled at the Chime level.
  BOOL isChimeEnabled =
      push_notification_settings::IsMobileNotificationsEnabledForAnyClient(
          base::SysNSStringToUTF8(identity.gaiaID), self.prefService);
  if (isChimeEnabled) {
    return true;
  }
  // Check the user's OS notification permission status for Chrome.
  __block UNAuthorizationStatus status;
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        status = settings.authorizationStatus;
      }];

  if (status != UNAuthorizationStatusNotDetermined &&
      status != UNAuthorizationStatusDenied) {
    return true;
  }
  return false;
}

- (BOOL)shouldShowNotificationsPromo {
  // Check if override is active. Override only works if the user is signed in.
  if (experimental_flags::ShouldForceContentNotificationsPromo()) {
    return true;
  }

  if (!IsContentNotificationExperimentEnabled() ||
      !IsContentNotificationPromoEnabled([self isUserSignedIn],
                                         self.isDefaultSearchEngine,
                                         self.prefService)) {
    return false;
  }

  // Check if notifications are enabled of any type at the Chime level.
  if ([self isNotificationsEnabled]) {
    return false;
  }

  int notificationsPromoTimesShown =
      self.prefService->GetInteger(prefs::kNotificationsPromoTimesShown);
  int notificationsPromoTimesDismissed =
      self.prefService->GetInteger(prefs::kNotificationsPromoTimesDismissed);

  base::Time now = base::Time::Now();
  // Check if promo has been displayed `kNotificationsPromoMaxShownCount`.
  if (notificationsPromoTimesShown >= kNotificationsPromoMaxShownCount) {
    [self enrollUserToProvisionalNotificationsFromEntrypoint:
              ContentNotificationPromoProvisionalEntrypoint::kShownThreshold];
    return false;
  }

  // Check if promo has been dismissed more than the threshold.
  if (notificationsPromoTimesDismissed >=
      kNotificationsPromoMaxDismissedCount) {
    return false;
  }
  // Check if the pref has been initialized before (base::Time() returns the
  // null value for a base::Time type.
  if (self.prefService->GetTime(prefs::kNotificationsPromoLastDismissed) !=
      base::Time()) {
    if (now -
            self.prefService->GetTime(prefs::kNotificationsPromoLastDismissed) <
        kNotificationsPromoDismissedCooldownTime) {
      return false;
    }
  }
  // Check if it has been less than `kNotificationsPromoShownCooldownTime`.
  if (now - self.prefService->GetTime(prefs::kNotificationsPromoLastShown) <
      kNotificationsPromoShownCooldownTime) {
    return false;
  }
  // If all the conditions pass above, update prefs and return true.
  self.prefService->SetTime(prefs::kNotificationsPromoLastShown, now);
  notificationsPromoTimesShown += 1;
  self.prefService->SetTime(prefs::kNotificationsPromoLastShown, now);
  self.prefService->SetInteger(prefs::kNotificationsPromoTimesShown,
                               notificationsPromoTimesShown);
  return true;
}

- (BOOL)shouldShowSigninPromo {
  // Don't show the promo if the account is not eligible for a SigninPromo.
  BOOL isAccountEligibleForSignInPromo = NO;
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO
                                    signinPromoAction:SigninPromoAction::
                                                          kInstantSignin
                                authenticationService:self.authenticationService
                                          prefService:self.prefService]) {
    isAccountEligibleForSignInPromo = ![self isUserSignedIn];
  }
  // Don't show the promo for incognito or start surface or if account is not
  // eligible.
  BOOL isStartSurfaceOrIncognito = self.isIncognito ||
                                   [self.NTPDelegate isStartSurface] ||
                                   !self.isSignInPromoEnabled;
  if (!isStartSurfaceOrIncognito && isAccountEligibleForSignInPromo) {
    return true;
  }
  return false;
}

- (void)updateShouldShowPromo {
  // Don't show any promo if Set Up List is Enabled.
  if (set_up_list_utils::IsSetUpListActive(
          GetApplicationContext()->GetLocalState(), self.prefService)) {
    // Hide promo as a safeguard in case it is being shown.
    [self.consumer hidePromo];
    return;
  }

  if ([self shouldShowSigninPromo]) {
    self.consumer.visiblePromoViewType = PromoViewTypeSignin;
    [self.consumer showPromo];
    return;
  }

  if ([self shouldShowNotificationsPromo]) {
    self.consumer.visiblePromoViewType = PromoViewTypeNotifications;
    [self.consumer showPromo];
    [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                    kDisplayed];
    return;
  }
}

#pragma mark - Private

- (void)enrollUserToProvisionalNotificationsFromEntrypoint:
    (ContentNotificationPromoProvisionalEntrypoint)entrypoint {
  [self logHistogramForEntrypoint:entrypoint];
  [ProvisionalPushNotificationUtil
      enrollUserToProvisionalNotificationsForClientIds:
          {PushNotificationClientId::kContent,
           PushNotificationClientId::kSports}
                           clientEnabledForProvisional:YES
                                       withAuthService:
                                           self.authenticationService
                                 deviceInfoSyncService:nil];
}

#pragma mark - Metrics

- (void)logHistogramForAction:(ContentNotificationTopOfFeedPromoAction)action {
  UmaHistogramEnumeration("ContentNotifications.Promo.TopOfFeed.Action",
                          action);
}

- (void)logHistogramForEntrypoint:
    (ContentNotificationPromoProvisionalEntrypoint)entrypoint {
  UmaHistogramEnumeration(
      "ContentNotifications.Promo.ProvisionalNotifications.Entrypoint",
      entrypoint);
}

@end
