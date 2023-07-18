// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_delegate.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WhatsNewInstructionsCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler>

// The view controller.
@property(nonatomic, strong) WhatsNewInstructionsViewController* viewController;
// What's New item.
@property(nonatomic, strong) WhatsNewItem* item;
// The delegate object that manages interactions with the primary action.
@property(nonatomic, weak) id<WhatsNewDetailViewActionHandler> actionHandler;

@end

@implementation WhatsNewInstructionsCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(WhatsNewItem*)item
                             actionHandler:(id<WhatsNewDetailViewActionHandler>)
                                               actionHandler {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    self.item = item;
    self.actionHandler = actionHandler;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[WhatsNewInstructionsViewController alloc]
      initWithWhatsNewItem:self.item];

  self.viewController.actionHandler = self;
  self.baseViewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.actionHandler didTapActionButton:self.item.type];
}

- (void)confirmationAlertSecondaryAction {
  [self.actionHandler didTapLearnMoreButton:self.item.learnMoreURL
                                       type:self.item.type];
  [self.delegate dismissWhatsNewInstructionsCoordinator:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate dismissWhatsNewInstructionsCoordinator:self];
}

@end
