// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_coordinator.h"

#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_mediator.h"
#import "ios/chrome/browser/assistant/gemini/ui/assistant_gemini_view_controller.h"

@implementation AssistantGeminiCoordinator {
  AssistantGeminiMediator* _mediator;
  AssistantGeminiViewController* _viewController;
}

- (UIViewController*)viewController {
  return _viewController;
}

- (void)start {
  _viewController = [[AssistantGeminiViewController alloc] init];
  _mediator = [[AssistantGeminiMediator alloc] init];
  _mediator.handler = self.handler;
  _mediator.consumer = _viewController;
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;
}

@end
