// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface TravelInfoCoordinator () <TravelInfoTableViewControllerDelegate>
@end

@implementation TravelInfoCoordinator {
  TravelInfoTableViewController* _viewController;
  TravelInfoMediator* _mediator;
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
  _viewController = [[TravelInfoTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  // The Travel Info setting page is only accessible if entityDataManager is
  // present.
  CHECK(entityDataManager);

  _mediator =
      [[TravelInfoMediator alloc] initWithEntityDataManager:entityDataManager];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - TravelInfoTableViewControllerDelegate

- (void)travelInfoTableViewControllerDidRemove:
    (TravelInfoTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate travelInfoCoordinatorDidRemove:self];
}

@end
