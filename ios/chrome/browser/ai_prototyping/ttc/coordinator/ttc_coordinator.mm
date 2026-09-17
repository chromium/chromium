// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_coordinator.h"

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"

@implementation TTCCoordinator {
  // View controller for TalkToChrome.
  TTCViewController* _viewController;

  // Mediator for TalkToChrome.
  TTCMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[TTCViewController alloc]
      initForFeature:AIPrototypingFeature::kTalkToChrome];
  _mediator = [[TTCMediator alloc] init];

  _mediator.consumer = _viewController;
  _viewController.ttcMutator = _mediator;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - Public

- (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  return _viewController;
}

@end
