---
name: ios/blocks-must-be-kept-short-single-method-call
description: In Objective-C blocks, keep implementations minimal by delegating to helper methods via weakSelf, avoiding ivar arrow dereferences, and verifying strongSelf non-nil when re-retention is required
rubric: safety
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Blocks
    quote: Moving the body into a private method avoids `weakSelf`/`strongSelf` dance bugs, prevents accidental ivar capture, and produces clear stack traces.
  - path: ios/best_practices.md
    heading: Properties and Instance Variables
    quote: Referencing `self` or `_ivar` inside a block strongly retains `self`. Always capture `__weak __typeof(self) weakSelf = self;`.
---

**Rule.** Keep Objective-C and Objective-C++ blocks concise by delegating
callbacks directly to a dedicated helper method on self or a standalone
function, reducing the block body down to a single invocation. In blocks
capturing weakSelf, never reference self directly or access bare ivars (\_ivar),
which creates retain cycles, and never access ivars via pointer dereference
(self->\_ivar, weakSelf->\_ivar, or strongSelf->\_ivar), which violates
encapsulation and risks segmentation faults if weakSelf is nil. Instead, capture
needed values in local variables before entering the block, or delegate state
access and mutation to a helper method on weakSelf. For single message sends
without side effects, message weakSelf directly (e.g., \[weakSelf
handleTransition\]) without introducing verbose strongSelf = weakSelf; if
(!strongSelf) return; boilerplate or redundant if (!weakSelf) checks. For
multi-step logic, multi-branch conditionals, nested callbacks, UI updates, or
configuration handlers (such as configurationUpdateHandler, actionHandler, or
cellProvider), delegate directly to a private helper method (\[weakSelf
helperMethod:...\]), relying on safe nil-messaging semantics to drop execution
safely if the object has deallocated. When block logic does not require self,
extract it into a standalone function adapted via base::CallbackToBlock.
Additionally, when explicit strongSelf re-retention is genuinely necessary—such
as for multi-step dependent operations that cannot be encapsulated in a single
helper method, or when passing nonnull arguments or objects with C++ move
semantics (e.g., std::move)—immediately verify that strongSelf is not nil (if
(!strongSelf) { return; }) before messaging strongSelf or executing further
logic.

**Why.** Inlining complex multi-step logic, UI configuration, or conditionals
inside blocks clutters call sites and obscures control flow. Referencing self or
bare ivars inside blocks creates accidental retain cycles. Dereferencing ivars
through pointers (weakSelf->\_ivar or strongSelf->\_ivar) violates object
encapsulation; furthermore, if weakSelf has deallocated, dereferencing
weakSelf->\_ivar crashes immediately with a segmentation fault. In contrast,
sending a message to nil in Objective-C is a well-defined no-op, allowing
single-call delegations on weakSelf to execute safely without verbose strongSelf
unwrapping boilerplate. Delegating multi-step operations to a helper method on
weakSelf ensures atomic execution for that call because the runtime retains self
for the duration of the method invocation. When strongSelf promotion is
unavoidable (such as when guarding C++ function calls or std::move operations
that would otherwise execute side effects regardless of receiver liveness),
checking if (!strongSelf) return; immediately prevents crashes and unintended
state changes on deallocated instances. Finally, extracting self-contained logic
into standalone functions adapted with base::CallbackToBlock makes dependencies
explicit and avoids capturing self altogether. See
`ios/best_practices.md#blocks` and
`ios/best_practices.md#properties-and-instance-variables`.

## Examples

Bad:

```cpp
// Bad: Ivar arrow dereference; direct self capture; redundant strongSelf unwrapping.
[actions addObject:[factory actionWithBlock:^{
  weakSelf->_dataCount = 10;
  [self.delegate fooViewController:self didSelectItem:self->_item];
}]];
auto completion = ^{
  MyVC* strongSelf = weakSelf;
  if (!strongSelf) return;
  [strongSelf handleTransition];
};
// Bad: Missing nil check after strongSelf promotion before side effects or C++ move.
__strong __typeof(weakSelf) strongSelf = weakSelf;
[strongSelf handleResult:std::move(records)];
```

Good:

```cpp
// Good: A single call on weakSelf; the helper owns the ivar mutation and the
// delegate callback.
[actions addObject:[factory actionWithBlock:^{
  [weakSelf didSelectCurrentItem];
}]];
auto completion = ^{
  [weakSelf handleTransition];
};
// Good: Immediately guard against nil when strongSelf promotion is genuinely required.
__strong __typeof(weakSelf) strongSelf = weakSelf;
if (!strongSelf) {
  return;
}
[strongSelf handleResult:std::move(records)];
```
