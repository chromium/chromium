---
name: ios/cache-bar-button-items-across-ui-updates
description: When updating navigation bars or toolbars during view controller state transitions, cache UIBarButtonItem instances in lazy getters or properties rather than recreating them
rubric: readability
severity: nit
---

**Rule.** In iOS view controllers, cache `UIBarButtonItem` instances (e.g.,
using lazily initialized getters or `@implementation` ivars) rather than
allocating or instantiating new buttons on each navigation bar or toolbar state
update. When transitioning between states (such as entering edit mode, updating
selection counts, or toggling search), reuse the cached button instances across
calls to UI update methods like `updateUIForEditState`.

**Why.** View controller update methods are invoked repeatedly during
interactive state changes, such as row selection toggles and edit mode
transitions. Re-instantiating `UIBarButtonItem` objects on each update pass
causes unnecessary heap allocations, triggers memory churn, and risks dropping
active button states or animations. Caching and reusing existing instances keeps
state transitions performant and smooth.

## Examples

Bad:

```cpp
- (void)updateUIForEditState {
  BOOL isEditing = self.tableView.editing;
  // Anti-pattern: Instantiating a new button on every state update.
  UIBarButtonItem* trailingButton =
      isEditing ? [self createEditModeDoneButtonForToolbar:YES]
                : [self selectButton];
  self.toolbarItems = @[ trailingButton ];
}
```

Good:

```cpp
- (void)updateUIForEditState {
  BOOL isEditing = self.tableView.editing;
  // Reuse the cached button instance across state updates.
  UIBarButtonItem* trailingButton =
      isEditing ? [self doneEditingButtonForToolbar] : [self selectButton];
  self.toolbarItems = @[ trailingButton ];
}

- (UIBarButtonItem*)doneEditingButtonForToolbar {
  if (!_doneEditingButtonForToolbar) {
    _doneEditingButtonForToolbar =
        [self createEditModeDoneButtonForToolbar:YES];
  }
  return _doneEditingButtonForToolbar;
}
```
