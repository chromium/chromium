---
name: ios/avoid-speculative-null-checks-on-invariant-dependencies
description: When accessing member variables, smart pointers, subviews, delegate properties, or invariant dependencies, omit speculative defensive null checks, ternary guards, and fallback branches
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: '`CHECK()`, `DCHECK()` and `NOTREACHED()` are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement *always* holds true at the point of `(D)CHECK`ing or that a piece of code is unreachable (for `NOTREACHED`). `CHECK` failures and reachable `NOTREACHED`s result in an application crash (generating a crash report).'
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: ProfileIOS objects correspond to a user browsing session. Each regular ProfileIOS also owns an incognito ProfileIOS. There can be more than one regular ProfileIOS loaded at the same time.
---

**Rule.** Do not add speculative defensive null/nil checks (`if (member_)` or
`member_ != nullptr`), ternary null guards (such as
`member_ ? member_->Method() : nullptr`), container emptiness guards (such as
`.empty()` or `size() == 0`), compound boolean guards, or dead fallback branches
for member variables, member pointers, smart pointers (such as
`std::unique_ptr`), collections, subviews, singletons, KeyedServices, or
delegate properties guaranteed present by class invariants or constructor
assertions (`CHECK()`). Likewise, omit defensive nil checks on newly
instantiated objects, such as coordinators and factory methods that allocate
valid instances. Profile-keyed services passed to UI coordinators or mediators
outlive the UI layer unless explicitly optional. Omit redundant conditionals and
empty fallback values for properties guaranteed by concrete models or delegates,
and avoid explicit readiness checks when query methods return safe defaults.
Only assert non-null at a use site when establishing a non-obvious lifecycle
invariant—using a bare `CHECK(cond)` when continuing cannot be made safe, or
`CHECK(cond, base::NotFatalUntil::M<n>)` (with `M<n>` set to the `MAJOR`
milestone in `//chrome/VERSION` plus 3 or 4, never copied from surrounding code)
followed by a graceful fallback when execution can safely recover—and clean up
temporary debugging guards before landing.

**Why.** Speculative null checks, ternary guards, redundant container emptiness
guards, readiness checks, and dead fallback branches for invariant dependencies,
collections, subviews, guaranteed delegate properties, member pointers, or newly
allocated coordinators introduce dead code and mislead reviewers into thinking
missing or empty states are possible. Accessing verified invariants,
unconditionally initialized members, and valid constructor/factory results
directly keeps control flow transparent, clarifies ownership contracts, and
avoids cognitive clutter. Unexpected contract violations should be surfaced via
explicit `CHECK` assertions (or `CHECK(cond, base::NotFatalUntil::M<n>)` with a
graceful fallback when execution can safely recover) rather than being silently
masked by unasserted no-ops or fallback values.

## Examples

Bad:

```cpp
// Variant 1: Defensive ternary or compound guard on guaranteed non-null member/service
web::WebState* webState =
    _webStateList ? _webStateList->GetActiveWebState() : nullptr;
if (_syncService &&
    _syncService->GetTransportState() == syncer::SyncService::TransportState::ACTIVE) {
  // ...
}

// Variant 2: Defensive runtime null check on unconditionally initialized smart pointer
if (startup_tracing_controller_) {
  startup_tracing_controller_->ShutdownAndWaitForStopIfNeeded();
}

// Variant 3: Defensive nil check on a newly allocated coordinator or subview
_fooCoordinator =
    [[FooCoordinator alloc] initWithBaseViewController:_viewController
                                               browser:self.browser];
if (!_fooCoordinator) {
  return;
}
```

Good:

```cpp
// Variant 1: Direct access relying on class invariant and non-null service
web::WebState* webState = _webStateList->GetActiveWebState();
if (_syncService->GetTransportState() == syncer::SyncService::TransportState::ACTIVE) {
  // ...
}

// Variant 2: Direct method dereference on unconditionally initialized member
startup_tracing_controller_->ShutdownAndWaitForStopIfNeeded();

// Variant 3: Direct coordinator invocation; alloc/init never returns nil here
_fooCoordinator =
    [[FooCoordinator alloc] initWithBaseViewController:_viewController
                                               browser:self.browser];
[_fooCoordinator start];
```
