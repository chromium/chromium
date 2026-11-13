---
name: ios/no-model-objects-in-view-controllers-or-views
description: In ios/chrome/browser, view controllers and views must not receive or store model- or application-layer objects (Browser, ProfileIOS, WebStateList, WebState, browser agents, tab helpers, keyed services) through initializers, properties, or instance variables
rubric: design
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Architecture Pattern Best Practices
    quote: Never pass model objects to the UI layer.
  - path: ios/best_practices.md
    heading: Architecture Pattern Best Practices
    quote: '**View Controllers and Views (`ui/`):** Manage layout and user interaction with zero model-layer imports.'
---

**Rule.** View controllers and views in `//ios/chrome/browser` must not receive
or store model- or application-layer objects, whether through initializers,
properties, or instance variables. This covers backbone containers such as
`Browser*`, `ProfileIOS*`, `AppState*`, `SceneState*`, `ProfileState*`, and
`CommandDispatcher*` as much as `WebStateList*`, `web::WebState*`, browser
agents, tab helpers, and keyed services. Mediators push UI-ready value types
through consumer protocols; coordinators wire mutators and narrow command
handlers for user actions.

**Why.** A view controller that holds a model object can read and mutate model
state directly, bypassing the mediator and the mutation-observation cycle, and
can no longer be tested with plain values. It also pulls model-layer headers and
`DEPS` into `ui/`. See
`ios/best_practices.md#architecture-pattern-best-practices`. For the types
passed across consumer and mutator protocols themselves, see
`ios/consumer-protocols-pass-simple-value-types-not-model-objects`.

## Examples

Bad:

```cpp
// ios/chrome/browser/foo/ui/foo_view_controller.h:
@interface FooViewController : UIViewController
- (instancetype)initWithBrowser:(Browser*)browser;
@end

// ios/chrome/browser/foo/ui/foo_view_controller.mm:
@implementation FooViewController {
  raw_ptr<WebStateList> _webStateList;
  raw_ptr<FooBrowserAgent> _fooBrowserAgent;
}
@end
```

Good:

```cpp
// ios/chrome/browser/foo/ui/foo_view_controller.h:
@interface FooViewController : UIViewController <FooConsumer>
@property(nonatomic, weak) id<FooMutator> mutator;
@end
```
