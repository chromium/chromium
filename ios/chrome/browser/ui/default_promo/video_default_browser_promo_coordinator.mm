// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/half_screen_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_mediator.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface VideoDefaultBrowserPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate,
    ConfirmationAlertActionHandler,
    HalfScreenPromoCoordinatorDelegate>

// The mediator for the video default browser promo.
@property(nonatomic, strong) VideoDefaultBrowserPromoMediator* mediator;
// The navigation controller.
@property(nonatomic, strong) UINavigationController* navigationController;
// The view controller.
@property(nonatomic, strong)
    VideoDefaultBrowserPromoViewController* viewController;
// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserPromoCommands>
    defaultBrowserPromoHandler;
// Half screen promo coordinator.
@property(nonatomic, strong)
    HalfScreenPromoCoordinator* halfScreenPromoCoordinator;

@end

@implementation VideoDefaultBrowserPromoCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserVideoPromo.Appear"));
  self.mediator = [[VideoDefaultBrowserPromoMediator alloc] init];
  self.navigationController = [[UINavigationController alloc] init];
  self.navigationController.presentationController.delegate = self;
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  if (self.isHalfScreen) {
    self.halfScreenPromoCoordinator = [[HalfScreenPromoCoordinator alloc]
        initWithBaseNavigationController:self.navigationController
                                 browser:self.browser];
    self.halfScreenPromoCoordinator.delegate = self;
    [self.halfScreenPromoCoordinator start];
  } else {
    [self showFullscreenVideoPromo];
  }

  [super start];
}

- (void)stop {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  if (self.halfScreenPromoCoordinator) {
    [self.halfScreenPromoCoordinator stop];
    self.halfScreenPromoCoordinator.delegate = nil;
    self.halfScreenPromoCoordinator = nil;
  }
  self.viewController = nil;
  self.mediator = nil;
  self.navigationController = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mediator didTapPrimaryActionButton];
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped);
  RecordAction(UserMetricsAction(
      "IOS.DefaultBrowserVideoPromo.Fullscreen.OpenSettingsTapped"));
  [self.handler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserVideoPromo.Fullscreen",
      IOSDefaultBrowserVideoPromoAction::kSecondaryActionTapped);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                IOSDefaultBrowserVideoPromoAction::kSwipeDown);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Dismiss"));
  [self.handler hidePromo];
}

#pragma mark - HalfScreenPromoCoordinatorDelegate

- (void)handlePrimaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  DCHECK(coordinator == self.halfScreenPromoCoordinator);
  [self.halfScreenPromoCoordinator stop];
  self.halfScreenPromoCoordinator.delegate = nil;
  self.halfScreenPromoCoordinator = nil;

  // Present sheet at full height.
  self.navigationController.sheetPresentationController.detents =
      @[ UISheetPresentationControllerDetent.largeDetent ];

  [self showFullscreenVideoPromo];
}

- (void)handleSecondaryActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  [self.handler hidePromo];
}

- (void)handleDismissActionForHalfScreenPromoCoordinator:
    (HalfScreenPromoCoordinator*)coordinator {
  [self.handler hidePromo];
}

#pragma mark - private

- (id<DefaultBrowserPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserPromoCommands);

  return handler;
}

- (void)showFullscreenVideoPromo {
  DCHECK(!self.viewController);
  RecordAction(
      UserMetricsAction("IOS.DefaultBrowserVideoPromo.Fullscreen.Impression"));
  self.viewController = [[VideoDefaultBrowserPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

@end
