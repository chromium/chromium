---
name: ios/omit-redundant-nil-check-before-message-send
rubric: readability
severity: should_fix
---

**Rule.** In Objective-C and Objective-C++, do not wrap method calls, property
accessors, object teardown (`[_coordinator stop]; _coordinator = nil;`), or
message dispatch in defensive `if (object)`, `if (object != nil)`,
`if (!object) return;`, or early-return guards when messaging `nil` safely
no-ops. When querying scalar, struct, or boolean properties, access them
directly instead of adding redundant `if (!view)` guards, compound checks
(`a && a.b`), or ternaries (`x ? x.b : NO`). Retain `nil` checks only when
guarding C++ function calls, non-nil side effects, non-id/non-scalar return
handling, or alternative control flow branches.

**Why.** The Objective-C runtime defines messaging `nil` as a well-defined no-op
that safely evaluates to void, `nil`, or zero (`0` for scalars, `CGRectZero` for
structs, and `NO` for `BOOL`). Defensive `nil` checks and early-return guards
add unnecessary branching and visual clutter without altering behavior or
providing safety benefits, and an early return placed before a message send
risks skipping the statements that follow it, such as teardown assignments,
completion handlers, or telemetry.

## Examples

Bad:

```cpp
// Bad: Redundant nil check or early return before message dispatch or teardown
if (!_pictureInPictureCoordinator) {
  return;
}
[_pictureInPictureCoordinator dismissAfterDisplay];

if (_coordinator) {
  [_coordinator stop];
  _coordinator = nil;
}

// Bad: Redundant compound check or ternary for nil-safe property access
if (self.navigationController && self.navigationController.navigationBar) {
  return YES;
}
BOOL allowed = _status ? _status.isEligibleByPolicy : NO;
```

Good:

```cpp
// Good: Direct message dispatch and teardown safely handling nil
[_pictureInPictureCoordinator dismissAfterDisplay];

[_coordinator stop];
_coordinator = nil;

// Good: Direct property access relying on nil-messaging semantics
if (self.navigationController.navigationBar) {
  return YES;
}
BOOL allowed = _status.isEligibleByPolicy;
```
