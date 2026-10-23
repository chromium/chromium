---
name: ios/delegate-properties-and-ivars-must-be-weak
description: When declaring Objective-C delegate, provider, observer, or command handler properties and ivars under ARC, qualify them with __weak or weak to prevent retain cycles
rubric: safety
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Protocol Naming and Design Conventions
    quote: Delegates must be held weakly by the delegating object.
  - path: https://google.github.io/styleguide/objcguide.html
    heading: Maintain Weak References to Avoid Retain Cycles
    quote: Delegate properties should be declared `weak` to avoid retain cycles.
---

**Rule.** In Objective-C and Objective-C++ classes under ARC, declare instance
variables and properties holding delegates, consumers, mutators, data sources,
providers, observers, or command handlers (such as `id<...Delegate>`,
`id<...Consumer>`, `id<...Mutator>`, `id<...DataSource>`, `id<...Provider>`,
`id<...Observing>`, or `id<...Commands>`) with `__weak` or
`@property(..., weak)`. Do not store non-owning collaborator interfaces or
command handlers in strong references or rely on default strong ivar ownership.
Additionally, when a coordinator configures its mediator or view controller with
command handlers (resolved via `HandlerForProtocol` from the coordinator's own
`CommandDispatcher`), ensure the receiving `<name>Handler` property or instance
variable is declared weak.

**Why.** Under Automatic Reference Counting (ARC), Objective-C instance
variables and properties default to `__strong` ownership. Because delegates,
consumers, observers, providers, and command handlers represent non-owning
dispatch targets or collaborator dependencies typically owned by callers, parent
coordinators, tab helpers, WebStates, or `CommandDispatcher`, storing them in
strong references creates retain cycles or artificially extends their lifetimes,
preventing proper deallocation and causing memory leaks when components or
coordinators are stopped or dismissed.

## Examples

Bad:

```cpp
@interface FeatureCoordinator : ChromeCoordinator
@property(nonatomic, strong) id<FeatureCoordinatorDelegate> delegate;
@end

@interface SettingsViewController : UIViewController
@property(nonatomic, strong) id<SettingsCommands> settingsHandler;
@end

@implementation FeatureMediator {
  // Retained strongly by default under ARC, creating retain cycles.
  id<FeatureConsumer> _consumer;
  id<GeminiCommands> _geminiHandler;
}
@end
```

Good:

```cpp
@interface FeatureCoordinator : ChromeCoordinator
@property(nonatomic, weak) id<FeatureCoordinatorDelegate> delegate;
@end

@interface SettingsViewController : UIViewController
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;
@end

@implementation FeatureMediator {
  __weak id<FeatureConsumer> _consumer;
  __weak id<GeminiCommands> _geminiHandler;
}
@end
```
