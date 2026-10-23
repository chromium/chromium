---
name: ios/guard-weak-let-with-swift-version
description: 'When declaring weak let variables in Swift, guard with #if swift(>=6.2.3) and fall back to weak var under #else on older compilers'
rubric: correctness
severity: should_fix
---

**Rule.** When declaring `weak let` variables (such as
`weak let weakSelf = self`) in Swift, wrap the declaration in
`#if swift(>=6.2.3)` with a fallback to `weak var` under `#else`. Do not use
`weak let` unguarded or guard with lower Swift version checks like
`#if swift(>=6.2)` or `#if swift(>=6.2.1)`, because `weak let` is not supported
on earlier compiler targets and will cause build failures.

**Why.** Compilers prior to Swift 6.2.3 do not support `weak let` declarations
and reject them as syntax errors. Using `#if swift(>=6.2.3)` with a fallback to
`weak var` maintains backward compatibility with older toolchains while allowing
newer compilers to adopt immutable weak bindings and avoid unmutated variable
warnings.

## Examples

Bad:

```cpp
// Anti-pattern 1: Unguarded weak let fails on compilers < 6.2.3.
weak let weakSelf = self

// Anti-pattern 2: Gating on 6.2 or 6.2.1 fails on earlier patch releases.
#if swift(>=6.2)
weak let weakSelf = self
#else
weak var weakSelf = self
#endif
```

Good:

```cpp
#if swift(>=6.2.3)
weak let weakSelf = self
#else
weak var weakSelf = self
#endif
```
