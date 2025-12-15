// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_mediator.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@implementation AssistantSheetCoordinator {
  AssistantSheetViewController* _viewController;
  AssistantSheetMediator* _mediator;
}

- (void)start {
  _viewController = [[AssistantSheetViewController alloc] init];

  _mediator = [[AssistantSheetMediator alloc] init];

  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  _viewController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _viewController.sheetPresentationController.prefersGrabberVisible = YES;

  // TODO(crbug.com/469050167): Implement full coordination logic.

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  _mediator = nil;
}

@end
