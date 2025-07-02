// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/coordinator/welcome_back_coordinator.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_detail_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/coordinator/welcome_back_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/ui/welcome_back_action_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/welcome_back_promo_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface WelcomeBackCoordinator () <ConfirmationAlertActionHandler,
                                      WelcomeBackActionHandler>
@end

@implementation WelcomeBackCoordinator {
  // Welcome Back mediator.
  WelcomeBackMediator* _mediator;
  // Base navigation controller.
  UINavigationController* _navigationController;
  // The BestFeaturesScreenDetail coordinator.
  BestFeaturesScreenDetailCoordinator* _detailScreenCoordinator;
  // Whether the user has tapped one of the Welcome Back items.
  BOOL _itemTapped;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];

  _mediator = [[WelcomeBackMediator alloc]
      initWithAuthenticationService:authenticationService
              accountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(profile)];

  // `kAutofillPasswordsInOtherApps` has been used if CPE is enabled on startup,
  // so remove the feature from the eligible features.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (password_manager_util::IsCredentialProviderEnabledOnStartup(localState)) {
    MarkWelcomeBackFeatureUsed(
        BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  }

  if (GetWelcomeBackEligibleItems().size() < 2) {
    [self hidePromo];
  } else {
    // TODO(crbug.com/407963758): Implement the Welcome Back Half Sheet view.
  }
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
}

#pragma mark - WelcomeBackActionHandler

- (void)didTapBestFeatureItem:(BestFeaturesItem*)item {
  feature_engagement::TrackerFactory::GetForProfile(self.profile)
      ->NotifyEvent(feature_engagement::events::kIOSWelcomeBackPromoUsed);
  _itemTapped = YES;

  _detailScreenCoordinator = [[BestFeaturesScreenDetailCoordinator alloc]
      initWithBaseNavigationViewController:_navigationController
                                   browser:self.browser
                          bestFeaturesItem:item];

  [_detailScreenCoordinator start];
}

#pragma mark - Private

// Dismisses the feature.
- (void)hidePromo {
  id<WelcomeBackPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), WelcomeBackPromoCommands);

  [handler hideWelcomeBackPromo];
}

@end
