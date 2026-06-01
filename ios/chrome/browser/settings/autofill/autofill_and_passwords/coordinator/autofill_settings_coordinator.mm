// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface AutofillSettingsCoordinator () <
    AutofillSettingsTableViewControllerDelegate>
@end

@implementation AutofillSettingsCoordinator {
  AutofillSettingsTableViewController* _viewController;
  AutofillSettingsMediator* _mediator;
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
  _viewController = [[AutofillSettingsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  _mediator = [[AutofillSettingsMediator alloc]
      initWithUserPrefService:self.browser->GetProfile()->GetPrefs()];
  _mediator.consumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - AutofillSettingsTableViewControllerDelegate

- (void)autofillSettingsTableViewControllerDidRemove:
    (AutofillSettingsTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate autofillSettingsCoordinatorDidRemove:self];
}

@end
