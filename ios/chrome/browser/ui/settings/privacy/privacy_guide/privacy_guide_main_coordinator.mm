// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator.h"

@interface PrivacyGuideMainCoordinator () <
    PrivacyGuideCommands,
    PrivacyGuideCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation PrivacyGuideMainCoordinator {
  UINavigationController* _navigationController;
  NSArray<NSNumber*>* _steps;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    // TODO: Not all steps in the list can be displayed. This will be handled
    // when optional steps are implemented.
    _steps = @[
      @(kPrivacyGuideWelcomeStep), @(kPrivacyGuideURLUsageStep),
      @(kPrivacyGuideHistorySyncStep), @(kPrivacyGuideSafeBrowsingStep)
    ];
  }
  return self;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PrivacyGuideCommands)];

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.navigationBar.accessibilityIdentifier =
      kPrivacyGuideNavigationBarViewID;
  _navigationController.presentationController.delegate = self;

  [self startNextCoordinator];
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
  [self startNextCoordinator];
}

- (void)dismissGuide {
  [self.delegate privacyGuideMainCoordinatorDidRemove:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate privacyGuideMainCoordinatorDidRemove:self];
}

#pragma mark - PrivacyGuideCoordinatorDelegate

- (void)privacyGuideCoordinatorDidRemove:(ChromeCoordinator*)coordinator {
  CHECK([self.childCoordinators containsObject:coordinator]);
  [coordinator stop];
  [self.childCoordinators removeObject:coordinator];
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

// Initializes the URL usage step coordinator and starts it.
- (void)startURLUsageCoordinator {
  PrivacyGuideURLUsageCoordinator* coordinator =
      [[PrivacyGuideURLUsageCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser];
  coordinator.delegate = self;
  [coordinator start];

  [self.childCoordinators addObject:coordinator];
}

// Initializes the History Sync step and starts it.
- (void)startHistorySyncCoordinator {
  PrivacyGuideHistorySyncCoordinator* coordinator =
      [[PrivacyGuideHistorySyncCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser];
  coordinator.delegate = self;
  [coordinator start];

  [self.childCoordinators addObject:coordinator];
}

// Initializes the Safe Browsing step and starts it.
- (void)startSafeBrowsingCoordinator {
  PrivacyGuideSafeBrowsingCoordinator* coordinator =
      [[PrivacyGuideSafeBrowsingCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser];
  [coordinator start];

  [self.childCoordinators addObject:coordinator];
}

- (void)startNextCoordinator {
  switch ([self nextStepType]) {
    case kPrivacyGuideWelcomeStep:
      [self startWelcomeCoordinator];
      break;
    case kPrivacyGuideURLUsageStep:
      [self startURLUsageCoordinator];
      break;
    case kPrivacyGuideHistorySyncStep:
      [self startHistorySyncCoordinator];
      break;
    case kPrivacyGuideSafeBrowsingStep:
      [self startSafeBrowsingCoordinator];
  }
}

- (PrivacyGuideStepType)nextStepType {
  NSUInteger index = self.childCoordinators.count;
  CHECK(index < _steps.count);

  return static_cast<PrivacyGuideStepType>([_steps[index] integerValue]);
}

// Stops all child coordinators and clears the child coordinator list.
- (void)stopAndCleanupChildCoordinators {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

@end
