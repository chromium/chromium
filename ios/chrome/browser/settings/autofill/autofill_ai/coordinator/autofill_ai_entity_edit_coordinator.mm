// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"

#import "base/check.h"
#import "base/types/optional_ref.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface AutofillAIEntityEditCoordinator ()

@property(nonatomic, strong) AutofillAIEntityEditMediator* mediator;

@end

@implementation AutofillAIEntityEditCoordinator {
  // The base navigation controller.
  UINavigationController* _baseNavigationController;

  // The entity ID.
  std::optional<autofill::EntityInstance::EntityId> _entityID;

  // The view controller.
  AutofillAIEntityEditTableViewController* _viewController;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        entityID:
                                            (autofill::EntityInstance::EntityId)
                                                entityID {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _entityID = entityID;
  }
  return self;
}

- (void)start {
  CHECK(_baseNavigationController);

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());

  CHECK(entityDataManager);
  CHECK(_entityID.has_value());

  base::optional_ref<const autofill::EntityInstance> entityInstanceOpt =
      entityDataManager->GetEntityInstance(*_entityID);

  if (!entityInstanceOpt.has_value()) {
    // Stop coordinator if entity not found.
    [self.delegate autofillAIEntityEditCoordinatorDidFinish:self];
    return;
  }

  self.mediator = [[AutofillAIEntityEditMediator alloc]
      initWithEntityInstance:*entityInstanceOpt
           entityDataManager:entityDataManager];

  _viewController = [[AutofillAIEntityEditTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.mediator.consumer = _viewController;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _mediator = nil;

  if (_viewController) {
    if (_viewController.navigationController) {
      [_baseNavigationController popViewControllerAnimated:YES];
    }
    _viewController = nil;
  }
}

@end
