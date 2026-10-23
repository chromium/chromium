---
name: ios/avoid-duplicate-invocations-for-conditional-arguments
description: When function calls, methods, or object instantiations are duplicated across conditional branches with differing receivers or arguments, resolve target or parameters and invoke once
rubric: design
severity: should_fix
---

**Rule.** Do not duplicate a function, method, delegate, telemetry logging call,
or object instantiation across conditional branches, switch cases, or ternary
expressions when only the receiver, fallback target, arguments, or configuration
values vary. Unconditionally resolve, evaluate, or adapt the varying target
receiver or parameters into local variables within each branch or in an inline
conditional expression (such as selecting a target receiver like `elementWindow`
or `window` before calling `getComputedStyle(...)`, passing conditional `IDS_*`
string IDs directly into `l10n_util::GetNSString(...)`, adapting an optional
callback with `base::CallbackToBlock` before passing to a completion parameter,
or selecting string values across switch cases). Perform the shared invocation,
configuration, or object construction (such as `[[alloc] init]` or
`make_unique`) only once outside the conditional structure.

**Why.** Duplicating invocations or object instantiations across branches or
switch cases just to supply differing target receivers or argument values
creates visual clutter, duplicates allocation boilerplate, and risks logic
divergence between branches. Decoupling target selection and parameter
adaptation from execution or object construction unifies execution paths, keeps
branches focused on state mapping, and consolidates the call site or
instantiation in a single place.

## Examples

Bad:

```cpp
// Duplicating lookup or delegate invocation across branches:
self.title = is_saved ? l10n_util::GetNSString(IDS_PAYMENTS)
                      : l10n_util::GetNSString(IDS_METHODS);
if (!callback) {
  [_delegate dismissWithCompletion:nil];
} else {
  [_delegate dismissWithCompletion:base::CallbackToBlock(std::move(callback))];
}

// Duplicating method call across branches with differing receiver:
if (elementWindow) {
  style = elementWindow.getComputedStyle(element);
} else if (element.ownerDocument === document) {
  style = window.getComputedStyle(element);
}
```

Good:

```cpp
// Resolve arguments before single call:
self.title = l10n_util::GetNSString(is_saved ? IDS_PAYMENTS : IDS_METHODS);
ProceduralBlock block =
    callback ? base::CallbackToBlock(std::move(callback)) : nil;
[_delegate dismissWithCompletion:block];

// Resolve receiver before single call:
if (!elementWindow && element.ownerDocument === document) {
  elementWindow = window;
}
if (elementWindow) {
  style = elementWindow.getComputedStyle(element);
}
```
