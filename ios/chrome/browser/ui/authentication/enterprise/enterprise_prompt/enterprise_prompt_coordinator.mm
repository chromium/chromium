// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if !TARGET_OS_MACCATALYST
namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace
#endif

@interface EnterprisePromptCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// ViewController that contains enterprise prompt information.
@property(nonatomic, strong) EnterprisePromptViewController* viewController;

// PromptType that contains the type of the prompt to display.
@property(nonatomic, assign) EnterprisePromptType promptType;

@end

@implementation EnterprisePromptCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                promptType:(EnterprisePromptType)promptType {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _promptType = promptType;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[EnterprisePromptViewController alloc]
      initWithpromptType:self.promptType];
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
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/1261423): Implement all cases.
  switch (self.promptType) {
    case EnterprisePromptTypeRestrictAccountSignedOut:
      [self.delegate enterprisePromptCoordinatorDidDismiss];
      break;
    case EnterprisePromptTypeForceSignOut:
    case EnterprisePromptTypeSyncDisabled:
      NOTREACHED();
      break;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate enterprisePromptCoordinatorDidDismiss];
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

@end
