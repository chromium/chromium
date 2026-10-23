---
name: ios/avoid-invariant-parameters-in-helpers
description: Avoid passing derivable, invariant, or speculative parameters to constructors, initializers, and helpers; hardcode constants and query container or receiver state internally.
rubric: design
severity: should_fix
---

**Rule.** Do not require callers to pass redundant, derivable, or invariant
parameters, speculative default arguments, or receiver state to constructors,
initializers, methods, or internal helpers when the callee can determine the
value internally:

- When an argument can be derived from other parameters (such as inferring a
  mode/scope enum like PushNotificationClientScope from ProfileIOS\*, inferring
  a boolean flag like isStandaloneMenu from pointer nilness such as
  UIInputState\*, or extracting properties like URLs from a passed container
  such as WebStateList), query or derive the value internally rather than
  accepting duplicate inputs.
- When all callers pass an identical constant or default value (such as boolean
  flags or underlying API options), hardcode the required behavior internally,
  store no unnecessary member flags, or pass constants directly to underlying
  API calls (annotating ambiguous literals like /*param=*/false). In specialized
  classes or overloads, encapsulate static configuration (such as fixed URLs),
  type discriminators, or source enums internally rather than exposing them as
  parameters.
- When callers pass values or properties already accessible on the receiver
  (self or this), query the member or property directly inside the helper rather
  than threading it as a redundant argument.

Only parameterize options or introduce configuration state when callers actively
require differing configurations.

**Why.** Passing redundant parameters, static configuration, or
container-derived properties duplicates information already present in other
arguments, bloats API signatures, and leaks internal model inspection logic into
callers while risking contradictory states. Accepting invariant parameters,
speculative default arguments, or redundant receiver state creates premature
configurability, clutters call sites and signatures with boilerplate arguments,
and increases cognitive overhead. Storing unneeded configuration flags in member
state can also introduce state hazards (such as asynchronous operations reading
stale or unexpectedly mutated flags) and misleading API contracts. Deriving
state from container arguments, hardcoding fixed constants, and querying
receiver state directly preserves encapsulation, enforces a single source of
truth, and keeps public and internal interfaces minimal and YAGNI-compliant.

## Examples

Bad:

```cpp
// Redundant parameter derivable from another argument or container:
PushNotificationClient(PushNotificationClientId client_id,
                       PushNotificationClientScope scope,
                       ProfileIOS* profile);
- (instancetype)initWithInputState:(UIInputState*)inputState
                  isStandaloneMenu:(BOOL)isStandaloneMenu;
_mediator = [[PasskeyCreationBottomSheetMediator alloc]
    initWithWebStateList:webStateList
                     URL:webStateList->GetActiveWebState()->GetLastCommittedURL()];

// Invariant parameter, speculative default, or static configuration:
CookieStoreIOS::CookieStoreIOS(std::unique_ptr<Store> store, bool fps);
void SetupContext(Callback cb, bool full_context = true);
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(NSURL*)fixedURL;

// Passing invariant constant or receiver state to helper method:
- (void)applySnapshotAnimatingDifferences:(BOOL)animated {
  [_dataSource applySnapshot:snapshot animatingDifferences:animated];
}
[self applySnapshotAnimatingDifferences:YES];
[self buildMenuWithEntitiesEnabled:[self canAddEntities]];
```

Good:

```cpp
// Query or infer properties directly inside constructor or initializer:
PushNotificationClient(PushNotificationClientId client_id,
                       ProfileIOS* profile);
- (instancetype)initWithInputState:(UIInputState*)inputState {
  if ((self = [super init])) {
    _isStandaloneMenu = (inputState == nil);
  }
  return self;
}
_mediator = [[PasskeyCreationBottomSheetMediator alloc]
    initWithWebStateList:webStateList];

// Omit invariant parameters, encapsulate fixed config, and pass constants directly:
CookieStoreIOS::CookieStoreIOS(std::unique_ptr<Store> store);
void SetupContext(Callback cb) {
  GenerateContext(std::move(cb), /*full_context=*/true);
}
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser;

// Hardcode fixed configuration and query receiver state directly inside helper:
- (void)applySnapshotAnimatingDifferences {
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}
- (UIMenu*)buildMenu {
  return [MenuBuilder buildWithEntities:[self canAddEntities] delegate:self];
}
```
