// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_coordinator.h"

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_coordinator.h"

@implementation CardUnmaskAuthenticationCoordinator {
  // This coordinator will present sub-coordinators in a UINavigationController.
  UINavigationController* _navigationController;

  // This sub-coordinator is used to prompts the user to select a particular
  // authentication method.
  CardUnmaskAuthenticationSelectionCoordinator* _selectionCoordinator;
}

- (void)start {
  _navigationController = [[UINavigationController alloc] init];
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

  // TODO(crbug.com/324114039): Determine which sub-coordinator needs to be
  // displayed. That is depending on how ChromeAutofillClientIOS sets up
  // AutofillBottomSheetTabHelper.
  _selectionCoordinator = [[CardUnmaskAuthenticationSelectionCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  [_selectionCoordinator start];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_selectionCoordinator stop];
  _selectionCoordinator = nil;
}

@end
