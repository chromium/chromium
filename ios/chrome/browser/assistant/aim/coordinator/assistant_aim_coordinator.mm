// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_mediator.h"
#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"

@implementation AssistantAIMCoordinator {
  AssistantAIMMediator* _mediator;
  AssistantAIMViewController* _viewController;
}

- (UIViewController*)viewController {
  return _viewController;
}

- (void)start {
  _viewController = [[AssistantAIMViewController alloc] init];
  _mediator = [[AssistantAIMMediator alloc] init];
  _mediator.handler = self.handler;
  _mediator.consumer = _viewController;
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;
}

@end
