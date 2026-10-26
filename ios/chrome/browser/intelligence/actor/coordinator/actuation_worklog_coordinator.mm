// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface ActuationWorklogCoordinator () <ActuationSuggestionsConsumer>
@end

@implementation ActuationWorklogCoordinator {
  // Mediator bridging task updates to the consumer.
  ActuationWorklogMediator* _mediator;
  // View controller displaying the worklog UI.
  ActuationWorklogViewController* _viewController;
  // Coordinator managing suggestion picker presentation.
  ActuationSuggestionsCoordinator* _suggestionsCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(self.browser);

  actor::ActorService* actorService =
      actor::ActorServiceFactory::GetForProfile(self.browser->GetProfile());

  _viewController = [[ActuationWorklogViewController alloc] init];
  _mediator =
      [[ActuationWorklogMediator alloc] initWithActorService:actorService];
  _mediator.consumer = _viewController;
  _mediator.suggestionsConsumer = self;
  _viewController.mutator = _mediator;
}

- (void)stop {
  [_suggestionsCoordinator stop];
  _suggestionsCoordinator = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController.suggestionsPresenter = nil;
  _viewController = nil;
}

#pragma mark - ActuationSuggestionsConsumer

- (void)setSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    forActiveWorklogItem:(ActuationWorklogItem*)worklogItem {
  CHECK(!_suggestionsCoordinator, base::NotFatalUntil::M166);
  _suggestionsCoordinator = [[ActuationSuggestionsCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                     suggestions:suggestions
                     worklogItem:worklogItem];
  [_suggestionsCoordinator start];
  _viewController.suggestionsPresenter = _suggestionsCoordinator;
}

- (void)clearSuggestions {
  [_suggestionsCoordinator stop];
  _suggestionsCoordinator = nil;
  _viewController.suggestionsPresenter = nil;
}

#pragma mark - Public

- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID {
  [_mediator startObservingTaskWithID:taskID];
}

- (void)stopObservingTask {
  [_mediator stopObservingTask];
}

#pragma mark - Properties

- (ActuationWorklogViewController*)viewController {
  return _viewController;
}

@end
