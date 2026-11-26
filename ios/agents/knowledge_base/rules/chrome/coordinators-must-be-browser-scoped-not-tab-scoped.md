---
name: ios/coordinators-must-be-browser-scoped-not-tab-scoped
description: Scope feature coordinators to a Browser by subclassing ChromeCoordinator (RootCoordinator only above the browser level), and never instantiate them per tab
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/coordinators.md
    heading: Coordinator Scope
    quote: "Almost all coordinators are **browser-scoped**: each is tied to a specific `Browser` instance (such as a window's regular or incognito browser) and manages UI for that browser."
  - path: ios/architecture/coordinators.md
    heading: Coordinator Scope
    quote: Coordinators are never instantiated per tab; per-tab model state is managed in the model layer (e.g., by tab helpers).
  - path: ios/architecture/coordinators.md
    heading: The Coordinator Hierarchy
    quote: Above the root of these browser-scoped coordinator hierarchies are a small number of coordinators that aren't browser-scoped (such as the scene's root coordinator, or coordinators whose UI spans multiple browsers).
  - path: ios/best_practices.md
    heading: Baseline Feature Support
    quote: Users can open multiple Chrome windows side by side. Never assume a single window or a single active tab list. Scope objects to a specific `SceneState` or `Browser` (see [Coordinator Scope](architecture/coordinators.md#coordinator-scope)).
---

**Rule.** Scope feature coordinators to a `Browser` by subclassing
`ChromeCoordinator`, and never instantiate a coordinator per tab. Even
short-lived single-tab UI coordinators take a `Browser`; only scene-level or
multi-browser coordinators above the browser hierarchy subclass
`RootCoordinator`. Keep per-tab model state in tab helpers and tab observation
in mediators, not on the coordinator.

**Why.** Each `ChromeCoordinator` manages UI for one `Browser` in a window and
reaches `CommandDispatcher`, browser agents, and profile services through that
`Browser`. Per-tab model state belongs in the model layer: tab helpers own it
and mediators observe tabs, which keeps coordinators free of model tracking. See
`ios/architecture/coordinators.md#coordinator-scope`,
`ios/architecture/coordinators.md#the-coordinator-hierarchy`, and
`ios/best_practices.md#baseline-feature-support`.

## Examples

Bad:

```cpp
// Variant 1: Per-tab coordinator initialized with a WebState instead of a
// Browser.
// ios/chrome/browser/feature/coordinator/tab_feature_coordinator.h
@interface TabFeatureCoordinator : NSObject
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                  webState:(web::WebState*)webState;
@end

// ios/chrome/browser/feature/coordinator/parent_coordinator.mm
_tabCoordinator = [[TabFeatureCoordinator alloc]
    initWithBaseViewController:self.baseViewController
                      webState:webState];

// Variant 2: Tracking or observing tabs directly on the coordinator instead of
// in tab helpers and a mediator.
@interface FeatureCoordinator () <CRWWebStateObserver, WebStateListObserving>
@property(nonatomic, assign) web::WebState* activeWebState;
@end
```

Good:

```cpp
// Variant 1: Subclass ChromeCoordinator so the coordinator is scoped to a
// Browser.
// ios/chrome/browser/feature/coordinator/feature_coordinator.h
@interface FeatureCoordinator : ChromeCoordinator
@end

// ios/chrome/browser/feature/coordinator/parent_coordinator.mm
_featureCoordinator = [[FeatureCoordinator alloc]
    initWithBaseViewController:self.baseViewController
                       browser:self.browser];
[_featureCoordinator start];

// Variant 2: Keep per-tab state in a tab helper and pass WebStateList* to the
// mediator in -start.
// ios/chrome/browser/feature/coordinator/feature_coordinator.mm
- (void)start {
  _viewController = [[FeatureViewController alloc] init];
  _mediator = [[FeatureMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  _mediator.consumer = _viewController;
}
```
