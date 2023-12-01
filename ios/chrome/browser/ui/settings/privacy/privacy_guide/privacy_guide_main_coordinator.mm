// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator.h"

@interface PrivacyGuideMainCoordinator () <
    PrivacyGuideCommands,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation PrivacyGuideMainCoordinator {
  UINavigationController* _navigationController;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PrivacyGuideCommands)];

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;

  [self startWelcomeCoordinator];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;

  [self stopAndCleanupChildCoordinators];
}

#pragma mark - PrivacyGuideCommands

- (void)showNextStep {
  // TODO(crbug.com/1488447): Implement showing the next Privacy Guide step.
}

- (void)dismissGuide {
  [self.delegate privacyGuideMainCoordinatorDidRemove:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate privacyGuideMainCoordinatorDidRemove:self];
}

#pragma mark - Private

// Initializes the Welcome step coordinator and starts it.
- (void)startWelcomeCoordinator {
  PrivacyGuideWelcomeCoordinator* coordinator =
      [[PrivacyGuideWelcomeCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser];
  [coordinator start];

  [self.childCoordinators addObject:coordinator];
}

// Stops all child coordinators and clears the child coordinator list.
- (void)stopAndCleanupChildCoordinators {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

@end
