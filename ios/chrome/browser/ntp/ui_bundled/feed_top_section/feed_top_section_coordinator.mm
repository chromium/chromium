// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@interface FeedTopSectionCoordinator () <
    SigninPresenter,
    NotificationsOptInAlertCoordinatorDelegate>

@property(nonatomic, strong) FeedTopSectionMediator* feedTopSectionMediator;
@property(nonatomic, strong)
    FeedTopSectionViewController* feedTopSectionViewController;
@property(nonatomic, strong) SigninPromoViewMediator* signinPromoMediator;

// Returns `YES` if the signin promo is visible in the NTP at the current scroll
// point.
@property(nonatomic, assign) BOOL isSigninPromoVisibleOnScreen;

// Returns `YES` if the signin promo exists on the current NTP.
@property(nonatomic, assign) BOOL isSignInPromoEnabled;

// Alert Coordinator used to display the notifications system prompt.
@property(nonatomic, strong)
    NotificationsOptInAlertCoordinator* optInAlertCoordinator;

@end

@implementation FeedTopSectionCoordinator

// Synthesized from ChromeCoordinator.
@synthesize viewController = _viewController;

- (void)start {
  DCHECK(self.NTPDelegate);
  self.feedTopSectionViewController =
      [[FeedTopSectionViewController alloc] init];
  _viewController = self.feedTopSectionViewController;

  ProfileIOS* profile = self.browser->GetProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  self.feedTopSectionMediator = [[FeedTopSectionMediator alloc]
      initWithConsumer:self.feedTopSectionViewController
       identityManager:identityManager
           authService:authenticationService
           isIncognito:profile->IsOffTheRecord()
           prefService:profile->GetPrefs()];
  self.isSignInPromoEnabled =
      ShouldShowTopOfFeedSyncPromo() && authenticationService &&
      [self.NTPDelegate isSignInAllowed] &&
      !authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);

  // If the user is signed out and signin is allowed, then start the top-of-feed
  // signin promo components.
  if (self.isSignInPromoEnabled) {
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(profile);
    self.signinPromoMediator = [[SigninPromoViewMediator alloc]
        initWithAccountManagerService:accountManagerService
                          authService:AuthenticationServiceFactory::
                                          GetForProfile(profile)
                          prefService:profile->GetPrefs()
                          syncService:syncService
                          accessPoint:signin_metrics::AccessPoint::
                                          ACCESS_POINT_NTP_FEED_TOP_PROMO
                      signinPresenter:self
             accountSettingsPresenter:nil];

    self.signinPromoMediator.signinPromoAction =
        SigninPromoAction::kSigninWithNoDefaultIdentity;
    self.signinPromoMediator.consumer = self.feedTopSectionMediator;
    self.feedTopSectionMediator.signinPromoMediator = self.signinPromoMediator;
    self.feedTopSectionViewController.signinPromoDelegate =
        self.signinPromoMediator;
  }

  const TemplateURL* defaultSearchURLTemplate =
      ios::TemplateURLServiceFactory::GetForProfile(profile)
          ->GetDefaultSearchProvider();

  bool isDefaultSearchEngine =
      defaultSearchURLTemplate && defaultSearchURLTemplate->prepopulate_id() ==
                                      TemplateURLPrepopulateData::google.id;
  self.feedTopSectionMediator.isDefaultSearchEngine = isDefaultSearchEngine;
  self.feedTopSectionMediator.presenter = self;
  self.feedTopSectionMediator.NTPDelegate = self.NTPDelegate;
  self.feedTopSectionViewController.delegate = self.feedTopSectionMediator;
  self.feedTopSectionViewController.feedTopSectionMutator =
      self.feedTopSectionMediator;
  self.feedTopSectionViewController.NTPDelegate = self.NTPDelegate;
  [self.feedTopSectionMediator setUp];
}

- (void)stop {
  _viewController = nil;
  [self.feedTopSectionMediator shutdown];
  [self.signinPromoMediator disconnect];
  self.signinPromoMediator.consumer = nil;
  self.signinPromoMediator = nil;
  self.feedTopSectionMediator = nil;
  self.feedTopSectionViewController = nil;
}

#pragma mark - Public

- (void)signinPromoHasChangedVisibility:(BOOL)visible {
  if (!self.isSignInPromoEnabled ||
      self.isSigninPromoVisibleOnScreen == visible) {
    return;
  }
  // Early return if the current promo State is SigninPromoViewState::kClosed
  // since the visibility shouldn't be updated if the Promo has been closed.
  // TODO(b/1494171): Update visibility methods to properlyhandle close actions.
  if (self.signinPromoMediator.signinPromoViewState ==
      SigninPromoViewState::kClosed) {
    return;
  }
  if (visible) {
    [self.signinPromoMediator signinPromoViewIsVisible];
  } else {
    [self.signinPromoMediator signinPromoViewIsHidden];
  }
  self.isSigninPromoVisibleOnScreen = visible;
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler showSignin:command baseViewController:self.baseViewController];
}

#pragma mark - Setters

- (void)setIsSignInPromoEnabled:(BOOL)isSignInPromoEnabled {
  _isSignInPromoEnabled = isSignInPromoEnabled;
  CHECK(self.feedTopSectionMediator);
  self.feedTopSectionMediator.isSignInPromoEnabled = isSignInPromoEnabled;
}

#pragma mark - NotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _optInAlertCoordinator.clientIds = std::vector{
      PushNotificationClientId::kContent, PushNotificationClientId::kSports};
  _optInAlertCoordinator.alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  _optInAlertCoordinator.confirmationMessage =
      l10n_util::GetNSString(IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_TITLE);
  _optInAlertCoordinator.delegate = self;
  [_optInAlertCoordinator start];
}

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_optInAlertCoordinator, alertCoordinator);
  std::vector<PushNotificationClientId> clientIds =
      alertCoordinator.clientIds.value();
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
  switch (result) {
    case NotificationsOptInAlertResult::kPermissionDenied:
      RecordAction(UserMetricsAction(
          "ContentNotifications.Promo.TopOfFeed.Permission.Declined"));
      [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                      kDecline];
      [self.feedTopSectionMediator updateFeedTopSectionWhenClosed];
      break;
    case NotificationsOptInAlertResult::kCanceled:
      [self logHistogramForEvent:ContentNotificationTopOfFeedPromoEvent::
                                     kCanceled];
      [self.feedTopSectionMediator updateFeedTopSectionWhenClosed];
      break;
    case NotificationsOptInAlertResult::kError:
      [self
          logHistogramForEvent:ContentNotificationTopOfFeedPromoEvent::kError];
      break;
    case NotificationsOptInAlertResult::kOpenedSettings:
      [self logHistogramForEvent:ContentNotificationTopOfFeedPromoEvent::
                                     kNotifActive];
      [self.feedTopSectionMediator updateFeedTopSectionWhenClosed];
      break;
    case NotificationsOptInAlertResult::kPermissionGranted:
      RecordAction(UserMetricsAction(
          "ContentNotifications.Promo.TopOfFeed.Permission.Accepted"));
      [self logHistogramForAction:ContentNotificationTopOfFeedPromoAction::
                                      kAccept];
      [self.feedTopSectionMediator updateFeedTopSectionWhenClosed];
      break;
  }
}

#pragma mark - Metrics

- (void)logHistogramForAction:(ContentNotificationTopOfFeedPromoAction)action {
  UmaHistogramEnumeration("ContentNotifications.Promo.TopOfFeed.Action",
                          action);
}

- (void)logHistogramForEvent:(ContentNotificationTopOfFeedPromoEvent)event {
  UmaHistogramEnumeration("ContentNotifications.Promo.TopOfFeed.Event", event);
}

@end
