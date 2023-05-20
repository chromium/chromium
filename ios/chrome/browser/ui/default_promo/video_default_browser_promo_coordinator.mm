// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_mediator.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VideoDefaultBrowserPromoCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate,
    ConfirmationAlertActionHandler>

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

@end

@implementation VideoDefaultBrowserPromoCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  self.mediator = [[VideoDefaultBrowserPromoMediator alloc] init];
  self.viewController = [[VideoDefaultBrowserPromoViewController alloc] init];
  self.viewController.actionHandler = self;
  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  self.navigationController.presentationController.delegate = self;
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
  [super start];
}

- (void)stop {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
  self.navigationController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mediator didTapPrimaryActionButton];
  [self.defaultBrowserPromoHandler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  [self.mediator didTapSecondaryActionButton];
  [self.defaultBrowserPromoHandler hidePromo];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.defaultBrowserPromoHandler hidePromo];
}

#pragma mark - private

- (id<DefaultBrowserPromoCommands>)defaultBrowserPromoHandler {
  id<DefaultBrowserPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserPromoCommands);

  return handler;
}

@end
