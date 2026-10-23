---
name: ios/coordinator-does-not-drive-mediator-business-logic
description: In iOS coordinators, delegate business logic, state tracking, service queries, and notifications to mediators rather than storing state or driving logic in coordinators
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

**Rule.** In iOS coordinators, delegate business logic, model state tracking,
service queries (such as clipboard content), system notification observers (such
as `UIApplicationDidBecomeActiveNotification`), and state mutations to
mediators. Do not query services, observe system notifications or model objects,
store feature/model state variables, compute UI state, or adopt consumer,
data-loading, or action protocols (such as `URLLoader`) in coordinators; connect
mediators directly to consumers. Coordinators must not evaluate model services
to configure UI in a feature that has a mediator, mutate models or browser
agents (such as resetting sessions), unpack payloads, or evaluate capacity
limits. Instead, inject services into mediators so mediators manage state,
observe services and notifications, and push UI state to consumers.
Additionally, when child coordinators or presentation transitions require state,
have the coordinator query the mediator's public state properties rather than
maintaining parallel state variables in the coordinator. Coordinators may still
read a small amount of static data in `-start` to configure a simple feature's
view controller when no mediator is otherwise needed (as soon as that data
requires observation, updates, or mutation, it belongs in a mediator), update a
mediator when a dependency the coordinator supplied changes, and pass results
returned by child coordinators, system pickers, or UIKit dismissal callbacks to
the mediator.

**Why.** In Chrome for iOS architecture, coordinators manage view presentation,
UI transitions, and scene lifecycles, while mediators encapsulate business
logic, state management, service queries, and model or system notification
observations. Querying services, tracking feature state variables, observing
system events, or mutating browser state in coordinators couples navigation to
domain state, turns coordinators into monolithic proxies, and duplicates
handling across entry points. Delegating state tracking and domain logic to
mediators preserves unidirectional data flow, ensures clean separation of
concerns, and enables independent unit testing of business logic without UI or
coordinator fixtures, while the narrow static-data and child-result exceptions
keep trivial features from needing a mediator they would not otherwise use. See
`ios/architecture/coordinators.md#core-guidelines-and-boundaries` and
`ios/architecture/coordinators.md#between-start-and-stop`.

## Examples

Bad:

```cpp
// Variant 1: Coordinator adopts consumer/loader or mutates agent directly
_mediator.consumer = self;
CobrowseBrowserAgent::FromBrowser(self.browser)->SetSessionActive(false);

// Variant 2: Coordinator evaluates a model service to configure UI state in a
// feature that already has a mediator
if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
  _viewController.hideStaySignedOutButton = YES;
}

// Variant 3: Coordinator queries services or observes system notifications
[self checkClipboardContent:^(std::set<ClipboardContentType> types) {
  [self monitorForChangesInClipboard:YES];
}];

// Variant 4: Coordinator tracks model state directly instead of delegating
@implementation ComposeboxInputPlateCoordinator {
  ComposeboxAttachmentSelection* _attachmentList;
}
- (void)presentMenu {
  _menuCoordinator = [[ComposeboxMenuCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
          preselectedAttachments:_attachmentList];
}
```

Good:

```cpp
// Variant 1: Connect consumer/loader directly to mediator and delegate mutations
_mediator.consumer = _viewController;
[_mediator endSession];

// Variant 2: Inject service into mediator; mediator evaluates and pushes to consumer
_mediator = [[AgeMismatchSignoutMediator alloc] initWithIdentityManager:identityMgr];
_mediator.consumer = _viewController;

// Variant 3: Delegate service queries and system notifications to mediator
[_mediator checkClipboardContent];
[_mediator beginMonitoringClipboard];

// Variant 4: Mediator owns state; coordinator queries mediator property
- (void)presentMenu {
  _menuCoordinator = [[ComposeboxMenuCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
          preselectedAttachments:_mediator.currentAttachmentSelection];
}

// Acceptable: Simple feature with no mediator reads static data once in -start
- (void)start {
  _viewController = [[LegalNoticeViewController alloc] init];
  _viewController.incognito = self.isOffTheRecord;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}
```
