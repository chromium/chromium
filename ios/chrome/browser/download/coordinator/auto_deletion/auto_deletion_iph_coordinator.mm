// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_iph_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_mediator.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_iph_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ui/base/device_form_factor.h"

@implementation AutoDeletionIPHCoordinator {
  // The ViewController for the Auto-deletion IPH.
  AutoDeletionIPHViewController* _viewController;
  // The mediator for auto-deletion.
  AutoDeletionMediator* _mediator;
  // The navigation controller containing the View Controller.
  UINavigationController* _navigationController;
}

- (void)start {
  _viewController =
      [[AutoDeletionIPHViewController alloc] initWithBrowser:self.browser];
  PrefService* localState = GetApplicationContext()->GetLocalState();
  _mediator = [[AutoDeletionMediator alloc] initWithLocalState:localState
                                                       browser:self.browser];
  _viewController.mutator = _mediator;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nullptr;
  _navigationController = nil;
}

@end
