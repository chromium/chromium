// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_coordinator.h"

#import "base/check_op.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface ShoppingCoordinator () <ShoppingTableViewControllerDelegate>
@end

@implementation ShoppingCoordinator {
  // The base navigation controller.
  UINavigationController* _baseNavigationController;

  // View controller providing the UI for the Shopping list.
  ShoppingTableViewController* _viewController;

  // Mediator providing the data and fulfilling mutator actions for the view.
  ShoppingMediator* _mediator;
}

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
  _viewController = [[ShoppingTableViewController alloc] init];
  _viewController.delegate = self;

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  // The Shopping setting page is only accessible if entityDataManager is
  // present.
  CHECK(entityDataManager);

  _mediator = [[ShoppingMediator alloc]
      initWithEntityDataManager:entityDataManager
                    prefService:self.browser->GetProfile()->GetPrefs()];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - ShoppingTableViewControllerDelegate

- (void)shoppingTableViewControllerDidRemove:
    (ShoppingTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate shoppingCoordinatorDidRemove:self];
}

@end
