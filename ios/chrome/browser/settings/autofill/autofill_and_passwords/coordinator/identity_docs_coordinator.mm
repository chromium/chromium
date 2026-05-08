// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface IdentityDocsCoordinator () <AutofillAIEntityEditCoordinatorDelegate,
                                       IdentityDocsMediatorDelegate,
                                       IdentityDocsTableViewControllerDelegate>
@end

@implementation IdentityDocsCoordinator {
  // View controller providing the UI for the Identity Docs list.
  IdentityDocsTableViewController* _viewController;

  // Mediator providing the data and fulfilling mutator actions for the view.
  IdentityDocsMediator* _mediator;

  // Coordinator for displaying and editing a selected identity doc entity.
  AutofillAIEntityEditCoordinator* _entityEditCoordinator;
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
  _viewController = [[IdentityDocsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  // The Identity Docs setting page is only accessible if entityDataManager is
  // present.
  CHECK(entityDataManager);

  _mediator = [[IdentityDocsMediator alloc]
      initWithEntityDataManager:entityDataManager];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [self stopEntityEditCoordinator];

  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - IdentityDocsTableViewControllerDelegate

- (void)identityDocsTableViewControllerDidRemove:
    (IdentityDocsTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate identityDocsCoordinatorDidRemove:self];
}

#pragma mark - IdentityDocsMediatorDelegate

- (void)identityDocsMediator:(IdentityDocsMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID {
  [self startEntityEditCoordinatorWithID:entityID];
}

#pragma mark - AutofillAIEntityEditCoordinatorDelegate

- (void)autofillAIEntityEditCoordinatorDidFinish:
    (AutofillAIEntityEditCoordinator*)coordinator {
  [self stopEntityEditCoordinator];
}

#pragma mark - Private

// Starts the coordinator responsible for displaying and editing the identity
// document entity with the specified ID.
- (void)startEntityEditCoordinatorWithID:
    (autofill::EntityInstance::EntityId)entityID {
  [self stopEntityEditCoordinator];
  _entityEditCoordinator = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              entityID:entityID];
  _entityEditCoordinator.delegate = self;
  [_entityEditCoordinator start];
}

// Stops and disconnects the active entity edit coordinator.
- (void)stopEntityEditCoordinator {
  [_entityEditCoordinator stop];
  _entityEditCoordinator.delegate = nil;
  _entityEditCoordinator = nil;
}

@end
