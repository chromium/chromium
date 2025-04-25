// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_coordinator.h"

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@interface ReaderModeCoordinator ()
@end

@implementation ReaderModeCoordinator {
  ReaderModeViewController* _viewController;
  ReaderModeMediator* _mediator;
}

- (void)start {
  _viewController = [[ReaderModeViewController alloc] init];
  _mediator = [[ReaderModeMediator alloc]
      initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()];
  _mediator.consumer = _viewController;
  [self.baseViewController addChildViewController:_viewController];
  [_viewController didMoveToParentViewController:self.baseViewController];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_viewController willMoveToParentViewController:nil];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
