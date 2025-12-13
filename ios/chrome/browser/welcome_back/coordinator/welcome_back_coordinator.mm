// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_detail_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/welcome_back_promo_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_mediator.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_action_handler.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface WelcomeBackCoordinator () <ConfirmationAlertActionHandler,
                                      WelcomeBackActionHandler,
                                      FirstRunScreenDelegate,
                                      UINavigationControllerDelegate,
                                      UIAdaptivePresentationControllerDelegate>
@end

@implementation WelcomeBackCoordinator {
  // Welcome Back mediator.
  WelcomeBackMediator* _mediator;
  // Welcome Back view controller.
  WelcomeBackViewController* _viewController;
  // Base navigation controller.
  UINavigationController* _navigationController;
  // The BestFeaturesScreenDetail coordinator.
  BestFeaturesScreenDetailCoordinator* _detailScreenCoordinator;
  // Number of time a feature was clicked in Welcome Back.
  int _featureClickedCount;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  _mediator = [[WelcomeBackMediator alloc]
      initWithAuthenticationService:authenticationService
              accountManagerService:accountManagerService];

  _viewController = [[WelcomeBackViewController alloc] init];
  _viewController.actionHandler = self;
  _viewController.welcomeBackActionHandler = self;

  _mediator.consumer = _viewController;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.navigationBarHidden = YES;
  _navigationController.delegate = self;
  _navigationController.presentationController.delegate = self;

  UISheetPresentationController* sheetController =
      _navigationController.sheetPresentationController;

  // Set both possible detents from the start.
  sheetController.detents = @[
    _viewController.preferredHeightDetent,
    [UISheetPresentationControllerDetent largeDetent]
  ];

  _featureClickedCount = 0;
  base::RecordAction(base::UserMetricsAction("IOS.WelcomeBack.Impression"));

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  base::RecordAction(base::UserMetricsAction("IOS.WelcomeBack.Stopped"));
  base::UmaHistogramCounts10000("IOS.WelcomeBack.FeaturesClickedCount",
                                _featureClickedCount);

  // Dismiss the presented view controller.
  if (_navigationController.presentingViewController &&
      !_navigationController.isBeingDismissed) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  // Clean up references.
  _viewController = nil;
  _navigationController = nil;
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

  // Ensure the sheet is at the large detent.
  [_navigationController.sheetPresentationController animateChanges:^{
    _navigationController.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }];

  _detailScreenCoordinator = [[BestFeaturesScreenDetailCoordinator alloc]
      initWithBaseNavigationViewController:_navigationController
                                   browser:self.browser
                          bestFeaturesItem:item];
  _detailScreenCoordinator.delegate = self;
  ++_featureClickedCount;
  base::UmaHistogramEnumeration("IOS.WelcomeBack.DetailScreen.Impression",
                                item.type);
  [_detailScreenCoordinator start];
}

#pragma mark - FirstRunScreenDelegate

- (void)screenWillFinishPresenting {
  // First dismiss the best feature detail view.
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_detailScreenCoordinator stop];
  _detailScreenCoordinator = nil;
  // Clean the promo.
  [self hidePromo];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  BOOL isWelcomeBackViewController =
      [viewController isKindOfClass:[WelcomeBackViewController class]];
  // When navigating back to the welcome back screen from a feature detail
  // screen.
  if (isWelcomeBackViewController && _detailScreenCoordinator) {
    _navigationController.navigationBarHidden = YES;
    [_detailScreenCoordinator stop];
    _detailScreenCoordinator = nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self hidePromo];
}

#pragma mark - Private

// Dismisses the promo by sending a command.
- (void)hidePromo {
  id<WelcomeBackPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), WelcomeBackPromoCommands);
  [handler hideWelcomeBackPromo];
}

@end
