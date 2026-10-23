---
name: ios/guard-optional-dereference-after-reset
description: When modifying code to reset or disengage an optional member variable, verify that all sibling methods accessing the member check it before dereferencing
rubric: correctness
severity: should_fix
---

**Rule.** When adding or modifying code that resets, clears, or disengages an
optional member variable (such as calling `.reset()` or assigning
`std::nullopt`), inspect all existing member methods and access sites across the
class to ensure they verify the optional contains a value before dereferencing
(`*opt` or `opt->`). Do not assume member optionals remain engaged across
transitions when new reset points are introduced.

**Why.** Dereferencing an unpopulated `std::optional` causes undefined behavior
and runtime crashes. Introducing a new `.reset()` point alters the class's
lifecycle invariants, making previously safe unconditional dereferences in
sibling methods into crash hazards.

## Examples

Bad:

```cpp
// In UpdateToBeCommittedRedirectChain:
pending_main_frame_query_.reset();

// In RedirectChainDecisionWithFilter:
if (redirect_chain == RedirectChain::kPendingMainFrame) {
  // Unsafe: pending_main_frame_query_ may now be nullopt after being reset above.
  if (pending_main_frame_query_->decision &&
      pending_main_frame_query_->decision->ShouldCancelNavigation()) {
    return pending_main_frame_query_->decision;
  }
}
```

Good:

```cpp
// In UpdateToBeCommittedRedirectChain:
pending_main_frame_query_.reset();

// In RedirectChainDecisionWithFilter:
if (redirect_chain == RedirectChain::kPendingMainFrame) {
  if (pending_main_frame_query_) {
    if (pending_main_frame_query_->decision &&
        pending_main_frame_query_->decision->ShouldCancelNavigation()) {
      return pending_main_frame_query_->decision;
    }
  }
}
```
