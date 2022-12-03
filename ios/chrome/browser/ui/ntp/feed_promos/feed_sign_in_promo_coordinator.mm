// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface FeedSignInPromoCoordinator () <ConfirmationAlertActionHandler>

@end

@implementation FeedSignInPromoCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  FeedSignInPromoViewController* signInPromoViewController =
      [[FeedSignInPromoViewController alloc] init];

  signInPromoViewController.actionHandler = self;

  if (@available(iOS 15, *)) {
    signInPromoViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        signInPromoViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    signInPromoViewController.modalPresentationStyle =
        UIModalPresentationFormSheet;
  }

  [self.baseViewController
      presentViewController:signInPromoViewController
                   animated:YES
                 completion:^(){
                     // TODO(crbug.com/1382615): add metrics.
                 }];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationSigninAndSync
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN];
  [handler showSignin:command baseViewController:self.baseViewController];
  // TODO(crbug.com/1382615): add metrics.
}

- (void)confirmationAlertSecondaryAction {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
  // TODO(crbug.com/1382615): add metrics.
}

@end
