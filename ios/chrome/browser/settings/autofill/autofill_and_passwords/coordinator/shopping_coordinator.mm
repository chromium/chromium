// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_coordinator.h"

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"

@interface ShoppingCoordinator () <AutofillAIEntityEditCoordinatorDelegate,
                                   AutofillAIBaseMediatorDelegate,
                                   ShoppingTableViewControllerDelegate,
                                   SuggestionsFromGeminiCoordinatorDelegate>
@end

@implementation ShoppingCoordinator {
  // The base navigation controller.
  UINavigationController* _baseNavigationController;

  // View controller providing the UI for the Shopping list.
  ShoppingTableViewController* _viewController;

  // Mediator providing the data and fulfilling mutator actions for the view.
  ShoppingMediator* _mediator;

  // Coordinator for displaying a selected shopping entity (read-only).
  AutofillAIEntityEditCoordinator* _entityEditCoordinator;

  // Coordinator for Suggestions from Gemini.
  SuggestionsFromGeminiCoordinator* _suggestionsFromGeminiCoordinator;
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

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _viewController.sceneHandler = HandlerForProtocol(dispatcher, SceneCommands);

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  // The Shopping setting page is only accessible if entityDataManager is
  // present.
  CHECK(entityDataManager);

  _mediator = [[ShoppingMediator alloc]
      initWithEntityDataManager:entityDataManager
                    prefService:self.browser->GetProfile()->GetPrefs()];
  _mediator.shouldShowSuggestionsFromGemini =
      autofill::ShouldShowPersonalContextAutofillSetting(
          self.browser->GetProfile());
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [self stopEntityEditCoordinator];
  [self stopSuggestionsFromGeminiCoordinator];

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

- (void)shoppingTableViewControllerDidSelectSuggestionsFromGemini:
    (ShoppingTableViewController*)controller {
  CHECK_EQ(_viewController, controller);
  base::RecordAction(base::UserMetricsAction(
      "PersonalContext.Settings.EntryPoint.ShoppingSettings"));
  [self startSuggestionsFromGeminiCoordinator];
}

#pragma mark - AutofillAIBaseMediatorDelegate

- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID {
  [self startEntityEditCoordinatorWithID:entityID];
}

- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToCreateEntityWithType:(autofill::EntityType)entityType {
  // No-op: Shopping entities cannot be created.
}

#pragma mark - AutofillAIEntityEditCoordinatorDelegate

- (void)autofillAIEntityEditCoordinatorDidFinish:
    (AutofillAIEntityEditCoordinator*)coordinator {
  [self stopEntityEditCoordinator];
}

#pragma mark - SuggestionsFromGeminiCoordinatorDelegate

- (void)suggestionsFromGeminiCoordinatorDidRemove:
    (SuggestionsFromGeminiCoordinator*)coordinator {
  CHECK_EQ(_suggestionsFromGeminiCoordinator, coordinator);
  [self stopSuggestionsFromGeminiCoordinator];
}

#pragma mark - Private

// Starts the Suggestions from Gemini sub-coordinator.
- (void)startSuggestionsFromGeminiCoordinator {
  [self stopSuggestionsFromGeminiCoordinator];
  _suggestionsFromGeminiCoordinator = [[SuggestionsFromGeminiCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser];
  _suggestionsFromGeminiCoordinator.delegate = self;
  [_suggestionsFromGeminiCoordinator start];
}

// Stops and disconnects the Suggestions from Gemini sub-coordinator.
- (void)stopSuggestionsFromGeminiCoordinator {
  [_suggestionsFromGeminiCoordinator stop];
  _suggestionsFromGeminiCoordinator.delegate = nil;
  _suggestionsFromGeminiCoordinator = nil;
}

// Starts the coordinator responsible for displaying the shopping entity
// with the specified ID.
- (void)startEntityEditCoordinatorWithID:
    (autofill::EntityInstance::EntityId)entityID {
  [self stopEntityEditCoordinator];
  _entityEditCoordinator = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
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
