// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_coordinator.h"

#import "base/feature_list.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_utils.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface FeedTopSectionCoordinator () <SigninPresenter>

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
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

@end

@implementation FeedTopSectionCoordinator

// Synthesized from ChromeCoordinator.
@synthesize viewController = _viewController;

- (void)start {
  DCHECK(self.NTPDelegate);
  self.feedTopSectionViewController =
      [[FeedTopSectionViewController alloc] init];
  _viewController = self.feedTopSectionViewController;

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);

  self.feedTopSectionMediator = [[FeedTopSectionMediator alloc]
      initWithConsumer:self.feedTopSectionViewController
       identityManager:identityManager
           authService:authenticationService
           isIncognito:browserState->IsOffTheRecord()
           prefService:browserState->GetPrefs()];
  self.isSignInPromoEnabled =
      ShouldShowTopOfFeedSyncPromo() && authenticationService &&
      [self.NTPDelegate isSignInAllowed] &&
      !authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);

  // If the user is signed out and signin is allowed, then start the top-of-feed
  // signin promo components.
  if (self.isSignInPromoEnabled) {
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
    self.signinPromoMediator = [[SigninPromoViewMediator alloc]
        initWithAccountManagerService:accountManagerService
                          authService:AuthenticationServiceFactory::
                                          GetForBrowserState(browserState)
                          prefService:browserState->GetPrefs()
                          syncService:syncService
                          accessPoint:signin_metrics::AccessPoint::
                                          ACCESS_POINT_NTP_FEED_TOP_PROMO
                      signinPresenter:self
             accountSettingsPresenter:nil];

    if (base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos)) {
      self.signinPromoMediator.signinPromoAction =
          SigninPromoAction::kSigninWithNoDefaultIdentity;
    }
    self.signinPromoMediator.consumer = self.feedTopSectionMediator;
    self.feedTopSectionMediator.signinPromoMediator = self.signinPromoMediator;
    self.feedTopSectionViewController.signinPromoDelegate =
        self.signinPromoMediator;
  }

  self.feedTopSectionMediator.messagePresenter = self;
  self.feedTopSectionMediator.notificationsPresenter = self;
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
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_SETTINGS_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_CONTENT_NOTIFICATIONS_PERMISSION_REDIRECT_ALERT_REDIRECT);

  __weak FeedTopSectionCoordinator* weakSelf = self;
  [_alertCoordinator stop];
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:_viewController
                                                   browser:self.browser
                                                     title:alertTitle
                                                   message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakSelf dimissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator
      addItemWithTitle:settingsTitle
                action:^{
                  [[UIApplication sharedApplication]
                                openURL:[NSURL URLWithString:settingURL]
                                options:{}
                      completionHandler:nil];
                  [weakSelf dimissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

#pragma mark - NotificationsConfirmationPresenter

- (void)presentNotificationsConfirmationMessage {
  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarHandler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_TITLE)
                   buttonText:
                       l10n_util::GetNSString(
                           IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_ACTION_MANAGE)
                messageAction:^{
                  [weakSelf showNotificationSettings];
                }
             completionAction:nil];
}

#pragma mark - Private

- (void)dimissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

// Display the notification settings.
- (void)showNotificationSettings {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      ApplicationSettingsCommands) showNotificationsSettings];
}

@end
