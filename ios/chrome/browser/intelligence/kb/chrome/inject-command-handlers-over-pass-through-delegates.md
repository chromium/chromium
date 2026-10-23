---
name: ios/inject-command-handlers-over-pass-through-delegates
description: When routing UI commands, dismissal, or teardown across coordinators, view controllers, or mediators, dispatch directly via HandlerForProtocol instead of pass-through or ad-hoc delegates
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Coordinator Delegates
    quote: A child coordinator requests its own teardown by invoking a command handled by its parent (see Standard Start and Stop Flow).
  - path: docs/ios/objects.md
    heading: Browser
    quote: The Browser also owns the CommandDispatcher and ChromeBroadcaster used for dispatching UI commands and property synchronisation.
---

**Rule.** Do not route UI commands, dismissal events, or teardown actions
through pass-through delegate trampolines or custom coordinator delegate
protocols across coordinator, view controller, or mediator layers when an
existing command protocol or `CommandDispatcher` handler is available.
Specifically:

1. Do not declare or implement ad-hoc coordinator delegate protocols solely to
   signal child coordinator dismissal, completion, or teardown (e.g.,
   `<Feature>CoordinatorDelegate`) when an equivalent command method exists or
   can be routed via a command protocol (such as `SceneCommands`,
   `BrowserCommands`, `PageInfoCommands`, or `QuickDeleteCommands`). Restrict
   coordinator delegates strictly to requesting teardown or returning modal
   results to parent coordinators when no standard command protocol applies.
2. Coordinators should invoke UI commands directly via
   `HandlerForProtocol(self.browser->GetCommandDispatcher(), CommandProtocol)`
   rather than defining relay delegates. Child coordinators are never handed
   handlers by their parent: they obtain their own from
   `self.browser->GetCommandDispatcher()`.
3. When a view controller or mediator dispatches a UI action or teardown command
   that needs no model data, the owning coordinator resolves the typed handler
   via `HandlerForProtocol(...)` and injects it into the component through a
   weak `<name>Handler` property (e.g.,
   `@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;`, never
   bare `handler`) or an initializer parameter, rather than creating
   pass-through delegate trampolines. A command that requires model data the
   view controller does not have (such as the `TabGroup` behind a group
   identifier) still goes view controller -> mutator -> mediator, and the
   mediator dispatches the command.

**Why.** In iOS Chrome architecture, coordinators decouple features and manage
lifecycles by routing UI actions, dismissal, and navigation through command
dispatchers. Creating custom single-purpose coordinator delegate protocols for
teardown or forwarding commands through intermediate coordinators and mediators
introduces boilerplate trampolines, tightly couples child coordinators to parent
coordinators, and duplicates the command mechanism. Mediators are designed for
model-layer business logic, not for relaying commands the view controller could
dispatch itself; they dispatch only the commands whose model-layer parameters
they alone can supply. Injecting typed command handlers directly from
`CommandDispatcher` into view controllers or mediators simplifies call stacks,
preserves architectural decoupling, and keeps coordinators focused on
presentation and lifecycle management.

## Examples

Bad:

```cpp
// Variant 1: Coordinator delegate for a dismissal already handled by a command.
@protocol ItemCoordinatorDelegate <NSObject>
- (void)itemCoordinatorDidFinish:(ItemCoordinator*)coordinator;
@end
- (void)closeSettings {
  [self.delegate sceneCoordinatorRequestsCloseSettings:self];
}

// Variant 2: Coordinator acts as a pass-through trampoline for its VC.
- (void)pageInfoViewControllerDidRequestDismissal:
    (PageInfoViewController*)viewController {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), PageInfoCommands)
      hidePageInfo];
}
```

Good:

```cpp
// Variant 1: Coordinator invokes the command handler directly.
id<SceneCommands> sceneHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), SceneCommands);
[sceneHandler closePresentedViews];

// Variant 2: Coordinator injects the weak handler; the view controller dispatches itself.
_viewController.pageInfoHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), PageInfoCommands);
// In PageInfoViewController: [self.pageInfoHandler hidePageInfo];

// Variant 3: A command needing model data goes through the mutator.
// In the view controller, which only has the group's identifier:
[self.mutator editGroupWithIdentifier:identifier];
// In the mediator, which resolves the model object and dispatches:
- (void)editGroupWithIdentifier:(tab_groups::TabGroupId)identifier {
  const TabGroup* group = [self groupWithIdentifier:identifier];
  [self.tabGroupsHandler showTabGroupEditionForGroup:group->GetWeakPtr()];
}
```
