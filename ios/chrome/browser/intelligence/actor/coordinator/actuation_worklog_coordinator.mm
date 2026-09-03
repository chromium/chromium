// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation ActuationWorklogCoordinator {
  // Mediator bridging task updates to the consumer.
  ActuationWorklogMediator* _mediator;
  // View controller displaying the worklog UI.
  ActuationWorklogViewController* _viewController;
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
  [_mediator connect];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - Properties

- (ActuationWorklogViewController*)viewController {
  return _viewController;
}

@end
