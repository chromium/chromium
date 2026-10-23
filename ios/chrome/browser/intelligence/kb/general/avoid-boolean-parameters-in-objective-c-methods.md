---
name: ios/avoid-boolean-parameters-in-objective-c-methods
description: Avoid bare bool, BOOL, or std::optional<bool> parameters and fields that multiplex behaviors or modes across Objective-C and C++ APIs; prefer dedicated methods, two-valued/tri-state enums, or base::StrongAlias
rubric: design
severity: should_fix
citations:
  - path: docs/patterns/prefer-enums.md
    heading: Prefer enums over bools for parameters
    quote: When you want to add a bool parameter to a function, consider giving the function a two-value enum parameter instead.
  - path: styleguide/c++/blink-c++.md
    heading: Prefer enums or StrongAliases to bare bools for function parameters
    quote: Prefer enums to bools for function parameters if callers are likely to be passing constants, since named constants are easier to read at the call site. Alternatively, you can use `base::StrongAlias<Tag, bool>`. An exception to this rule is a setter function, where the name of the function already makes clear what the boolean is.
---

**Rule.** Avoid bare boolean (`BOOL`/`bool`) parameters on Objective-C or C++
methods, functions, initializers, ivars, and configuration options when the
boolean multiplexes distinct behaviors, query scopes, modes, styles, policies,
UI variations, or lifecycles. Prefer, in order of fit:

- **Dedicated methods**: Split multiplexed operational phases (such as setup
  versus completion cleanup, especially when companion parameters are ignored
  depending on the flag) into self-contained methods, provide distinct
  descriptively named methods for each query scope or filter, provide
  convenience methods without the boolean so standard call sites do not pass
  boilerplate `YES`/`NO` literals, and use paired action methods
  (`begin...`/`end...`, `start...`/`stop...`) for operations that start and
  stop.
- **Dedicated initializers**: When a class supports distinct initialization
  modes with different parameter requirements, define separate designated or
  convenience initializers with descriptive selector names instead of a mode
  flag, so callers never pass dummy, empty, or `nil` placeholders for
  inapplicable parameters and each initializer enforces its own preconditions.
- **Descriptive enums**: When a parameter selects between modes, styles, types,
  or policies (particularly when wrapping a component that itself exposes an
  enum), use an `enum class` with descriptive enumerators or
  `base::StrongAlias<Tag, bool>`; for tri-state options, policies, fallback
  behaviors, or test overrides (automatic vs. force vs. prevent; default vs.
  force-enabled vs. force-disabled), use an explicit `enum class` dispatched
  through an exhaustive `switch` rather than `std::optional<bool>`. When a mode
  enum is shared across layers (such as a coordinator and a view controller),
  declare it in a dedicated header in `ui/` (e.g., `*_mode.h`) to avoid header
  coupling.

Simple setters whose name makes the boolean obvious (`set_visible(bool)`,
`setHidden:`) remain valid.

**Why.** Boolean flag parameters introduce control coupling and obscure caller
intent at invocation sites: literals like `YES`/`NO` or `foo(true, false)` carry
no semantic meaning without the declaration, can be transposed without compiler
warnings, and restrict an API from expanding beyond two states; in Objective-C,
which lacks default parameter values, they also force callers to pass unlabelled
literals for standard behavior. Multiplexing setup versus completion cleanup or
distinct initialization modes leaves companion arguments dead or invalid in
alternative branches, requiring dummy placeholders and weakening precondition
enforcement. `std::optional<bool>` is ambiguous for tri-state policies because
`std::nullopt`, `true`, and `false` lack domain semantics and because
`std::optional` is contextually convertible to `bool`, so `if (opt)` tests
presence rather than the inner value (true even when holding `false`). Dedicated
methods, separate initializers, paired action methods, and explicit `enum class`
values keep call sites self-documenting, accommodate future values without
signature churn, enable compiler-enforced exhaustive `switch` checks, and align
with the configurations of wrapped components.

## Examples

Bad:

```cpp
// Multiplexing setup vs completion cleanup with ignored parameters:
- (void)updateToolsMenuButtonVisibility:(BOOL)visibility
                          useCompletion:(BOOL)useCompletion {
  if (!useCompletion) {
    self.headerView.toolsMenuButton.hidden = NO;
    self.headerView.toolsMenuButton.alpha = visibility ? 1.0 : 0.0;
  } else {
    self.headerView.toolsMenuButton.hidden =
        self.headerView.toolsMenuButton.alpha == 0.0;
  }
}

// Initializer forcing empty dummy arguments for an alternative mode:
- (instancetype)initWithConsumer:(id<Consumer>)consumer
                    selectedTabs:(const std::set<web::WebStateID>&)tabs
            createNewTabForGroup:(BOOL)createNewTabForGroup;
[[CreateTabGroupMediator alloc] initWithConsumer:c selectedTabs:{} createNewTabForGroup:YES];

// Lifecycle toggling with a boolean flag:
- (void)monitorClipboard:(BOOL)shouldMonitor;

// Bare boolean literals obscure intent and can be transposed; a mode flag
// restricts API expansion:
void SetupTask(bool enable_sandbox, bool record_stats);
- (instancetype)initWithColors:(NSArray<UIColor*>*)colors
              useEasedGradient:(BOOL)easedGradient;

// std::optional<bool> invites contextual bool-conversion bugs:
if (_overrideForTesting) {  // Bug: true even when holding false!
  return *_overrideForTesting;
}
```

Good:

```cpp
// Dedicated methods for each phase, each a single call from the completion block:
- (void)updateToolsMenuButtonVisibility:(BOOL)visibility {
  self.headerView.toolsMenuButton.hidden = NO;
  self.headerView.toolsMenuButton.alpha = visibility ? 1.0 : 0.0;
}
- (void)hideToolsMenuButtonIfTransparent {
  self.headerView.toolsMenuButton.hidden =
      self.headerView.toolsMenuButton.alpha == 0.0;
}
// In the completion block at the call site:
[weakSelf hideToolsMenuButtonIfTransparent];

// Dedicated initializers tailored to each mode without dummy arguments:
- (instancetype)initWithConsumer:(id<Consumer>)consumer
                    selectedTabs:(const std::set<web::WebStateID>&)tabs;
- (instancetype)initEmptyGroupWithConsumer:(id<Consumer>)consumer;
[[CreateTabGroupMediator alloc] initEmptyGroupWithConsumer:c];

// Paired action methods making lifecycle call sites self-describing:
- (void)beginMonitoringClipboard;
- (void)endMonitoringClipboard;

// Enums self-document call sites and accommodate future modes; simple setters
// remain valid:
enum class SandboxMode { kDisabled, kEnabled };
void SetupTask(SandboxMode sandbox_mode, RecordStats record_stats);
void set_visible(bool visible);
- (instancetype)initWithColors:(NSArray<UIColor*>*)colors
                  gradientType:(GradientLayerType)gradientType;

// Explicit enum class for tri-state overrides, dispatched exhaustively:
enum class TestOverride { kNotOverridden, kForceEnabled, kForceDisabled };
switch (_overrideForTesting) {
  case TestOverride::kNotOverridden:
    return [self checkDefault];
  case TestOverride::kForceEnabled:
    return YES;
  case TestOverride::kForceDisabled:
    return NO;
}
```
