---
name: ios/comment-empty-method-bodies-with-noop
description: Document intentional no-ops with `// No-op.` instead of empty bodies or logging macros, but use `NOTREACHED()` for uncallable stubs and required subclass overrides
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: NOTIMPLEMENTED and NOTREACHED logging macros
    quote: "`NOTREACHED`: This function should not be called. If it is, we have a problem somewhere else. `NOTIMPLEMENTED`: This isn't implemented because we don't use it yet. If it's called, then we need to figure out what it should do. When something is called but doesn't need an implementation, just add a comment indicating this instead of using a logging macro."
---

**Rule.** When defining an intentionally empty function or method body—such as
in required Objective-C protocol conformances, delegate callbacks, handlers,
interface overrides, prototypes, or test doubles (fakes and stubs)—add an
explicit inline comment (such as `// No-op.` or
`// Required by <Protocol> protocol.`) explaining why the body is empty. Do not
leave the body completely blank, and do not use `NOTIMPLEMENTED()` or
`NOTREACHED()` logging macros when receiving calls without an implementation is
expected behavior at runtime (such as reachable interface overrides called by
shared components or unmanaged prototype branches; return early or no-op with an
explanatory comment instead). Conversely, when defining method bodies that
should never be invoked at runtime, fail fast by calling `NOTREACHED()`
accompanied by an explanatory comment:

1. In base class methods or virtual handlers that subclasses are required to
   override (such as Objective-C abstract base classes or handlers required upon
   opting into a capability), call `NOTREACHED()` with a comment (e.g.,
   `// This method must be implemented by child class.`) rather than providing
   empty stubs, silent no-op fallbacks, dummy error responses, or cross-handler
   delegations.
2. In stub or provider implementations where a feature is unsupported and
   guarded by capability checks (e.g., `IsSupported()` returns false), call
   `NOTREACHED()` in forbidden methods rather than leaving silent empty bodies
   so caller precondition violations fail fast.
3. For `@optional` protocol methods, omit the method entirely instead of adding
   an empty stub, so that `respondsToSelector:` reports the truth to the caller.

**Why.** Per docs/ios/style.md, `NOTREACHED()` asserts that code cannot be
executed and results in a fatal crash, while `NOTIMPLEMENTED()` logs unexpected
calls to unfinished functionality. Using `NOTREACHED()` or `NOTIMPLEMENTED()` in
expected no-op callbacks, reachable interface overrides, or prototypes causes
false-positive test failures, misleading log spam, and runtime application
crashes. Conversely, leaving method bodies completely blank looks like an
accidental omission or forgotten TODO, inviting review churn; adding an explicit
comment like `// No-op.` documents deliberate design intent. However, when code
should never be executed, silent empty bodies and dummy fallbacks hide bugs: -
Objective-C classes lack compile-time pure virtual methods; empty base stubs
silently do nothing if invoked directly or if a subclass fails to override them.
Similarly, in C++ virtual handlers, silent fallbacks, cross-handler delegation,
or dummy error strings mask missing subclass implementations. - In unsupported
provider APIs guarded by capability checks (e.g., `IsSupported()` returns
false), silent empty bodies mask caller precondition violations. Calling
`NOTREACHED()` enforces API contracts and ensures unexpected invocations fail
fast at runtime, while inline comments clarify the subclass or capability
requirements.

## Examples

Bad:

```cpp
// 1. Empty body or logging macro on reachable no-op:
- (void)setSubtitle:(NSString*)subtitle {}
- (void)updateAccessibilityStatus {
  NOTIMPLEMENTED();
}

// 2. Base method requiring subclass override left empty or as fallback:
- (void)bannerInfobarButtonWasPressed:(id)sender {}
void Feature::OnMessage(Msg msg, Callback cb) {
  if (!cb.is_null()) std::move(cb).Run(nullptr, @"Unsupported");
}

// 3. Silent empty body when caller must check capability guard:
void PrimesStartLogging() {}  // Caller should check IsPrimesSupported().
```

Good:

```cpp
// 1. Explicit comment documenting reachable no-op:
- (void)setSubtitle:(NSString*)subtitle {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}
- (void)updateAccessibilityStatus {
  // No-op.
}

// 2. Call NOTREACHED() with comment in base methods requiring override:
- (void)bannerInfobarButtonWasPressed:(id)sender {
  // This method must be implemented by child class.
  NOTREACHED();
}
void Feature::OnMessage(Msg msg, Callback cb) {
  // Subclasses handling messages with reply must override this method.
  NOTREACHED();
}

// 3. Call NOTREACHED() when caller must check capability guard:
void PrimesStartLogging() {
  NOTREACHED();
}
```
