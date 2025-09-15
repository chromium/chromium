// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_container_coordinator.h"

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_coordinator.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_entrypoint.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_container_view_controller.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_dismiss_animator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_present_animator.h"

@interface AIMPrototypeContainerCoordinator () <
    UIViewControllerTransitioningDelegate>

@end

@implementation AIMPrototypeContainerCoordinator {
  // The coordinator for the main AIM UI.
  AIMPrototypeCoordinator* _aimCoordinator;
  // The entrypoint that triggered the AIM prototype.
  AIMPrototypeEntrypoint _entrypoint;
  // An optional query to pre-fill the omnibox.
  NSString* _query;
  // The container view controller.
  AIMPrototypeContainerViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(AIMPrototypeEntrypoint)entrypoint
                                     query:(NSString*)query {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
  }
  return self;
}

- (void)start {
  _viewController = [[AIMPrototypeContainerViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;

  _aimCoordinator = [[AIMPrototypeCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      entrypoint:_entrypoint
                           query:_query];
  [_aimCoordinator start];

  [_viewController addInputViewController:_aimCoordinator.inputViewController];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;

  [_aimCoordinator stop];
  _aimCoordinator = nil;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  AIMPrototypePresentAnimator* animator = [[AIMPrototypePresentAnimator alloc]
      initWithContextProvider:_aimCoordinator.contextProvider];
  animator.toggleOnAIM = _entrypoint == AIMPrototypeEntrypoint::kNTPAIMButton;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[AIMPrototypeDismissAnimator alloc]
      initWithContextProvider:_aimCoordinator.contextProvider];
}

@end
