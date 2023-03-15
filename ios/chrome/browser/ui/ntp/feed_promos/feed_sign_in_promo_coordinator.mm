// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_coordinator.h"

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface FeedSignInPromoCoordinator () <ConfirmationAlertActionHandler>

// Metrics recorder for actions relating to the feed.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;

@end

@implementation FeedSignInPromoCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  DCHECK(IsFeedCardMenuSignInPromoEnabled());

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  // Show sign-in only flow when there's one or more device-level user account.
  if (accountManagerService->HasIdentities()) {
    [self showSignInFlowWithSignInOnly:YES];
    return;
  }

  self.feedMetricsRecorder = DiscoverFeedServiceFactory::GetForBrowserState(
                                 self.browser->GetBrowserState())
                                 ->GetFeedMetricsRecorder();

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
                 completion:^() {
                   const signin_metrics::AccessPoint access_point =
                       signin_metrics::AccessPoint::
                           ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO;
                   signin_metrics::
                       RecordSigninImpressionUserActionForAccessPoint(
                           access_point);
                 }];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.feedMetricsRecorder recordSignInPromoUIContinueTapped];
  if (self.baseViewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf showSignInFlowWithSignInOnly:NO];
                           }];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.feedMetricsRecorder recordSignInPromoUICancelTapped];
  [self stop];
}

#pragma mark - Helpers

- (void)showSignInFlowWithSignInOnly:(BOOL)signInOnly {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO;
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:signInOnly ? AuthenticationOperationSigninOnly
                                   : AuthenticationOperationSigninAndSync
            accessPoint:access_point];
  signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
  [handler showSignin:command baseViewController:self.baseViewController];
}

@end
