---
name: ios/avoid-notreached-in-ui-action-handlers
description: When implementing UI interaction handlers for displayable states or error types, do not use NOTREACHED(); handle the state gracefully or suppress the UI element
rubric: correctness
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: NOTIMPLEMENTED and NOTREACHED logging macros
    quote: "`NOTREACHED`: This function should not be called. If it is, we have a problem somewhere else. `NOTIMPLEMENTED`: This isn't implemented because we don't use it yet. If it's called, then we need to figure out what it should do."
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: reachable `NOTREACHED`s result in an application crash (generating a crash report).
---

**Rule.** Do not mark states, error codes, or enum branches with `NOTREACHED()`
in UI action or event handlers if those states can be presented to the user.
When a state or error can trigger the display of an interactive UI element (such
as an error button, dialog action, or menu item), either implement graceful
handling for that state or prevent the UI element from being displayed.

**Why.** `NOTREACHED()` asserts that control flow cannot reach that point and
crashes the application with a fatal failure if executed. Because any state
presented in the UI can be triggered by legitimate user interactions, marking
its action handler with `NOTREACHED()` introduces deterministic,
user-triggerable application crashes.

## Examples

Bad:

```cpp
- (void)didTapErrorButton {
  switch (_error.type) {
    case SyncError::kBookmarksLimitExceeded:
      NOTREACHED();
    case SyncError::kSignInNeedsUpdate:
      [self.syncErrorHandler showSignInUpdate];
      break;
  }
}
```

Good:

```cpp
- (void)didTapErrorButton {
  switch (_error.type) {
    case SyncError::kBookmarksLimitExceeded:
      [self.syncErrorHandler showBookmarksLimitHelp];
      break;
    case SyncError::kSignInNeedsUpdate:
      [self.syncErrorHandler showSignInUpdate];
      break;
  }
}
```
