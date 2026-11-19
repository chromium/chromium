---
name: ios/coordinator-does-not-drive-mediator-business-logic
description: Keep business logic, model and notification observation, service queries, and model or browser-agent mutations in the feature's mediator rather than in its coordinator.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Core Guidelines and Boundaries
    quote: 'Keep coordinators focused on coordination. Do not allow coordinators to become catch-all controllers: Push business logic and model observation/mutation into mediators.'
  - path: ios/architecture/coordinators.md
    heading: Between Start and Stop
    quote: 'Once started, a feature mostly runs itself: the view controller and mediator communicate through consumer and mutator protocols, and reach the rest of the app through commands.'
---

**Rule.** Coordinators don't contain business logic. Observing models or system
notifications, querying services, mutating models or browser agents, and the
state that results all belong in the feature's mediator, which the coordinator
creates with the services it needs. After `-start`, a coordinator only presents
and dismisses UI, updates its mediator when a dependency changes, and passes
child results to it. A feature simple enough to need no mediator may read a
small amount of static data in `-start`.

**Why.** Coordinators manage feature lifecycle, presentation, and dependency
wiring, while mediators encapsulate business logic and model interactions.
Keeping domain state and service calls out of coordinators prevents them from
becoming catch-all controllers. It also lets business logic be unit-tested in
the mediator without view controllers or coordinator fixtures. See
`ios/architecture/coordinators.md#core-guidelines-and-boundaries` and
`ios/architecture/coordinators.md#between-start-and-stop`.

## Examples

Bad:

```cpp
// Variant 1: Coordinator mutates a browser agent in response to a user action.
// foo_coordinator.mm
- (void)userDidTapEndSession {
  CobrowseBrowserAgent::FromBrowser(self.browser)->SetSessionActive(false);
}

// Variant 2: In a feature that has a mediator, the coordinator queries
// services and observes notifications to update UI state.
// foo_coordinator.mm
- (void)start {
  ...
  [self updateStaySignedOutButton];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateStaySignedOutButton)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
}

- (void)updateStaySignedOutButton {
  ProfileIOS* profile = self.profile;
  _viewController.hideStaySignedOutButton =
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin);
}
```

Good:

```cpp
// Variant 1: Coordinator wires the mutator in -start; the view controller
// sends user actions directly to the mediator, which mutates the agent.
// foo_coordinator.mm
- (void)start {
  _viewController = [[FooViewController alloc] init];
  _mediator = [[FooMediator alloc]
      initWithCobrowseAgent:CobrowseBrowserAgent::FromBrowser(self.browser)];
  _viewController.mutator = _mediator;
  ...
}

// foo_view_controller.mm
- (void)didTapEndSessionButton {
  [self.mutator endSession];
}

// foo_mediator.mm
- (void)endSession {
  _cobrowseAgent->SetSessionActive(false);
}

// Variant 2: Coordinator injects services in -start; the mediator observes
// notifications on its own, pushes state to its consumer, and cleans up in
// -disconnect.
// foo_coordinator.mm
- (void)start {
  _viewController = [[FooViewController alloc] init];
  _mediator = [[FooMediator alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                  self.profile)];
  _mediator.consumer = _viewController;
  ...
}

// foo_mediator.mm
- (instancetype)initWithIdentityManager:
    (signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateStaySignedOutButton)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }

  return self;
}

- (void)disconnect {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _identityManager = nullptr;
}

- (void)setConsumer:(id<FooConsumer>)consumer {
  _consumer = consumer;
  [self updateStaySignedOutButton];
}

- (void)updateStaySignedOutButton {
  if (!_consumer || !_identityManager) {
    return;
  }

  [_consumer setStaySignedOutButtonHidden:
                 _identityManager->HasPrimaryAccount(
                     signin::ConsentLevel::kSignin)];
}
```
