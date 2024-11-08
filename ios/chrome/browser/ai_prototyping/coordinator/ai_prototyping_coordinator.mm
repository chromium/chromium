// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_coordinator.h"

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

@interface AIPrototypingCoordinator () {
  // The mediator for handling AI prototyping models.
  AIPrototypingMediator* _mediator;

  // The view controller presented as the AI protoyping menu.
  AIPrototypingViewController* _viewController;
}

@end

@implementation AIPrototypingCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[AIPrototypingMediator alloc]
      initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()];

  // TODO(crbug.com/370768381): Remove web state reference from view controller.
  _viewController = [[AIPrototypingViewController alloc]
      initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()];

  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;
}

@end
