---
name: ios/avoid-notreached-in-ui-action-handlers
rubric: correctness
severity: should_fix
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
