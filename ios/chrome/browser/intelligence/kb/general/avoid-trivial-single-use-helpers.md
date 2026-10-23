---
name: ios/avoid-trivial-single-use-helpers
description: When writing private or anonymous-namespace helpers, inline single-caller logic, isolated branches, 1-to-1 forwarding wrappers, property forwarding, or binary mappings directly instead of adding indirection
rubric: design
severity: should_fix
---

**Rule.** Do not introduce or retain private helper methods or
anonymous-namespace (file-scope) functions that contain trivial logic, act as
1-to-1 forwarding wrappers around existing helpers, perform simple binary value
mappings, forward member variables/properties, or wrap smart pointer cleanup.
Inline trivial logic or binary mappings (e.g., using a ternary operator)
directly at the call site, consolidate redundant forwarding wrappers into a
single canonical helper, and rely on RAII for cleanup. Additionally, do not
define or retain separate helper functions that have only a single caller when
they merely isolate a branch, delegate an operation, or handle a single call
site without providing meaningful abstraction or reuse; merge or inline the
implementation directly into its sole caller. Reserve dedicated private helpers
for complex, multi-step logic whose decomposition significantly improves caller
readability, or access instance state directly when separating concerns. A
helper is justified, regardless of how trivial its body looks, when it meets any
of these criteria: it has two or more callers; it names a non-obvious condition
or computation; or it hosts a block body, that is, it is the single method
called on `weakSelf` from inside a block (`[weakSelf fooDidComplete:]`) or the
helper called on `weakSelf` instead of dereferencing `weakSelf->_ivar`. Private
subview-construction helpers that a `UIView` subclass calls from `-init` are
also kept. A single-caller wrapper that merely renames an expression is inlined.

**Why.** Trivial 1-to-1 forwarding wrappers, binary mappings, property
forwarding, single-caller branch delegations, and smart pointer destruction
wrappers introduce unnecessary indirection, cognitive jumps, and function
boilerplate without providing real abstraction, testability, or modularity.
Inlining logic, ternary expressions, or single-caller branches directly into the
caller keeps control flow linear, co-located, and easy to follow, while
consolidating redundant wrappers and relying on standard RAII cleanly
communicates ownership. Helpers that host a block body are different: moving the
body out of the block into a private method is what keeps blocks to a single
call on `weakSelf`, avoids capturing `self` or ivars, and makes a `nil`
`weakSelf` a safe no-op, so inlining them would reintroduce retain-cycle and
null-dereference bugs.

## Examples

Bad:

```cpp
// Variant 1: Single-caller helper merely isolating a branch.
UIColor* DefaultColor() { return [UIColor colorNamed:kGrey300]; }
UIColor* ButtonColor(bool dark) {
  return dark ? [UIColor colorNamed:kGrey900] : DefaultColor();
}

// Variant 2: Redundant property or member forwarding wrapper.
- (CGFloat)topPadding { return self.topInset; }

// Variant 3: Trivial binary mapping or smart pointer destruction helper.
AuthOp GetOp(bool signed_in) {
  return signed_in ? AuthOp::kSync : AuthOp::kSheet;
}
void DestroyState(std::unique_ptr<WebState> s) { s.reset(); }

// Variant 4: Inlining a single-caller helper into a block body; the block now
// captures self and an ivar.
[service performOperationWithCompletion:^(BOOL success) {
  _operationSucceeded = success;
  [self.consumer setLoading:NO];
}];
```

Good:

```cpp
// Variant 1: Inline single-caller branch directly into caller.
UIColor* ButtonColor(bool dark) {
  return dark ? [UIColor colorNamed:kGrey900] : [UIColor colorNamed:kGrey300];
}

// Variant 2: Access underlying property directly at call site.
self.offsetConstraint.constant = self.topInset;

// Variant 3: Inline ternary mapping and rely on RAII scope destruction.
AuthOp op = signed_in ? AuthOp::kSync : AuthOp::kSheet;
std::unique_ptr<WebState> destroyed = [self releaseContents];

// Variant 4: Keep the single-caller helper that hosts the block body; the
// block stays a single call on weakSelf.
__weak __typeof(self) weakSelf = self;
[service performOperationWithCompletion:^(BOOL success) {
  [weakSelf operationDidCompleteWithSuccess:success];
}];

- (void)operationDidCompleteWithSuccess:(BOOL)success {
  _operationSucceeded = success;
  [self.consumer setLoading:NO];
}
```
