---
name: ios/avoid-redundant-table-view-begin-end-updates
description: When updating table view edit state or layout transitions, avoid calling beginUpdates and endUpdates to refresh cell heights unless cells dynamically change height.
rubric: readability
severity: nit
---

**Rule.** Do not invoke empty `[tableView beginUpdates]` and
`[tableView endUpdates]` blocks during `UITableView` state updates (such as
toggling edit mode or updating toolbar actions) unless table view cells
dynamically change height in response to that state change.

**Why.** Calling `-[UITableView beginUpdates]` immediately followed by
`-[UITableView endUpdates]` forces `UITableView` to recalculate row heights and
trigger layout animation passes. When cell heights remain constant across state
transitions, empty update blocks add unnecessary CPU layout overhead and
animation churn.

## Examples

Bad:

```cpp
- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updateToolbarForEditState];
  // Redundant: cells in this view controller have fixed heights in edit mode.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}
```

Good:

```cpp
- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updateToolbarForEditState];
}
```
