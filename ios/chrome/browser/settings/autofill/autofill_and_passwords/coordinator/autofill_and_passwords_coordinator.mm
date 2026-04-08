// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_coordinator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface AutofillAndPasswordsCoordinator () <
    AutofillAndPasswordsTableViewControllerDelegate>
@end

@implementation AutofillAndPasswordsCoordinator {
  AutofillAndPasswordsTableViewController* _viewController;
  AutofillAndPasswordsMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillAndPasswordsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  _mediator = [[AutofillAndPasswordsMediator alloc] init];
  _mediator.consumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _viewController = nil;
}

#pragma mark - AutofillAndPasswordsTableViewControllerDelegate

- (void)autofillAndPasswordsTableViewControllerDidRemove:
    (UIViewController*)controller {
  [self.delegate autofillAndPasswordsCoordinatorDidRemove:self];
}

@end
