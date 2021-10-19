// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_signout/enterprise_signout_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_signout/enterprise_signout_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if !TARGET_OS_MACCATALYST
namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace
#endif

@interface EnterpriseSignoutCoordinator () <
    ConfirmationAlertActionHandler,
    IdentityManagerObserverBridgeDelegate,
    UIAdaptivePresentationControllerDelegate> {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}
// ViewController that contains enterprise signout information.
@property(nonatomic, strong) EnterpriseSignoutViewController* viewController;

// Dispatcher to present the sign in screen.
@property(nonatomic, strong) id<ApplicationCommands> handler;

// YES if the sign-in is in progress.
@property(nonatomic, assign) BOOL isSigninInProgress;

@end

@implementation EnterpriseSignoutCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];

  if (self) {
    _handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                  ApplicationCommands);
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(
                self.browser->GetBrowserState()),
            self);
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[EnterpriseSignoutViewController alloc] init];
  self.viewController.presentationController.delegate = self;
  self.viewController.actionHandler = self;

#if !TARGET_OS_MACCATALYST
  if (@available(iOS 15, *)) {
    self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.viewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
#else
  {
#endif
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self dismissSignOutViewController];
  _identityManagerObserver.reset();
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate enterpriseSignoutCoordinatorDidDismiss];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate enterpriseSignoutCoordinatorDidDismiss];
}

#pragma mark - Private

// Remove view controller from display.
- (void)dismissSignOutViewController {
  if (self.viewController) {
    [self.baseViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    self.viewController = nil;
  }
}

// Presents the sign-in dialog to the user.
- (void)showSignIn {
  DCHECK(self.handler);
  DCHECK(!self.isSigninInProgress);
  self.isSigninInProgress = YES;

  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(BOOL success) {
                 [weakSelf didFinishSignin];
               }];
  [self.handler showSignin:command baseViewController:self.baseViewController];
}

// Updates isSigninInProgress when sign-in process is done.
- (void)didFinishSignin {
  DCHECK(self.isSigninInProgress);
  self.isSigninInProgress = NO;
  [self.delegate enterpriseSignoutCoordinatorDidDismiss];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!self.isSigninInProgress)
        [self.baseViewController dismissViewControllerAnimated:true
                                                    completion:nil];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

@end
