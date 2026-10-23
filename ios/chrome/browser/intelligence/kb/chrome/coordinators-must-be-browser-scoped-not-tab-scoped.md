---
name: ios/coordinators-must-be-browser-scoped-not-tab-scoped
description: When managing tab or web page UI, do not instantiate coordinators per tab; coordinators must be browser-scoped and observe WebStateList.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Coordinator Scope
    quote: 'Almost all coordinators are browser-scoped: each is tied to a specific Browser instance and manages UI for that browser. Coordinators are never instantiated per tab; per-tab model state is managed in the model layer.'
---

**Rule.** Coordinators must be browser-scoped rather than tab-scoped: a
coordinator is created with a `Browser` and is never instantiated per tab.
Tab-specific model state belongs in tab helpers; a feature whose UI follows the
active tab observes the browser's `WebStateList` from its mediator (the
coordinator hands `self.browser->GetWebStateList()` to the mediator in
`-start`), never from the coordinator itself.

**Why.** Instantiating coordinators per tab duplicates UI controllers and wastes
memory. Browser-scoped coordinators provide stable UI lifecycles across tab
switches, and keeping the `WebStateList` observation in the mediator preserves
the coordinator's role as a pure lifecycle manager. See
`ios/architecture/coordinators.md#coordinator-scope` and
`ios/best_practices.md#baseline-feature-support`.

## Examples

Bad:

```cpp
// Per-tab coordinator instantiation
- (void)webStateWasCreated:(web::WebState*)webState {
  _tabCoordinator = [[TabCoordinator alloc] initWithWebState:webState];
}

// Browser-scoped, but the coordinator observes the model itself.
@interface FeatureCoordinator () <WebStateListObserving>
@end
```

Good:

```cpp
// Browser-scoped coordinator; the mediator follows the active tab.
- (void)start {
  _mediator = [[FeatureMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  _viewController = [[FeatureViewController alloc] init];
  _mediator.consumer = _viewController;
}

// In FeatureMediator (conforms to WebStateListObserving via a
// WebStateListObserverBridge held in a std::unique_ptr ivar):
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self updateForWebState:status.new_active_web_state];
  }
}
```
