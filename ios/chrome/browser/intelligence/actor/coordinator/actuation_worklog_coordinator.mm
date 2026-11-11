// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface ActuationWorklogCoordinator () <ActuationSuggestionsConsumer>
@end

@implementation ActuationWorklogCoordinator {
  // Mediator bridging task updates to the consumer.
  ActuationWorklogMediator* _mediator;
  // View controller displaying the worklog UI.
  ActuationWorklogViewController* _viewController;
  // Coordinator managing suggestion picker presentation.
  ActuationSuggestionsCoordinator* _suggestionsCoordinator;
  // Suggestions presented by `_suggestionsCoordinator`.
  NSArray<ActorFormSuggestion*>* _presentedSuggestions;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(self.browser);

  _viewController = [[ActuationWorklogViewController alloc] init];
  _mediator = [[ActuationWorklogMediator alloc] init];
  _mediator.consumer = _viewController;
  _mediator.suggestionsConsumer = self;
  _viewController.mutator = _mediator;
}

- (void)stop {
  [self stopSuggestionsCoordinator];
  [_mediator disconnect];
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController.suggestionsPresenter = nil;
  _viewController = nil;
}

#pragma mark - ActuationSuggestionsConsumer

- (void)setSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    forActiveWorklogItem:(ActuationWorklogItem*)worklogItem {
  // The presented `suggestions` are matched by their elements, since a data
  // source may resend them in a different array.
  if ([suggestions isEqualToArray:_presentedSuggestions]) {
    _suggestionsCoordinator.worklogItem = worklogItem;
    return;
  }
  [self stopSuggestionsCoordinator];
  if (!suggestions) {
    return;
  }
  _presentedSuggestions = [suggestions copy];
  _suggestionsCoordinator = [[ActuationSuggestionsCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                     suggestions:suggestions
                     worklogItem:worklogItem];
  _suggestionsCoordinator.delegate = _mediator;
  [_suggestionsCoordinator start];
  _viewController.suggestionsPresenter = _suggestionsCoordinator;
}

#pragma mark - Public

- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource
                   actionHandler:(id<ActorTaskActionHandler>)actionHandler {
  [_mediator startObservingDataSource:dataSource actionHandler:actionHandler];
}

- (void)stopObservingDataSource {
  [self stopSuggestionsCoordinator];
  [_mediator stopObservingDataSource];
}

#pragma mark - Properties

- (ActuationWorklogViewController*)viewController {
  return _viewController;
}

#pragma mark - Private

// Stops presenting the suggestions, if any.
- (void)stopSuggestionsCoordinator {
  [_suggestionsCoordinator stop];
  _suggestionsCoordinator = nil;
  _presentedSuggestions = nil;
  _viewController.suggestionsPresenter = nil;
}

@end
