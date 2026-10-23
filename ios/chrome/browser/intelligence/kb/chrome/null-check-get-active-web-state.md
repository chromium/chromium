---
name: ios/null-check-get-active-web-state
description: When querying or operating on the active WebState in WebStateList, verify GetActiveWebState() is non-null and active_index() is not kInvalidIndex before dereferencing or branching
rubric: correctness
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: WebStateList
    quote: WebStateList represents a list of WebStates. It maintains a notion of an active WebState and opener-opened relationship between WebStates.
---

**Rule.** When accessing the active tab in a `WebStateList`, verify that
`GetActiveWebState()` is non-null before dereferencing and guard against
`active_index() == WebStateList::kInvalidIndex` before invoking index-based
lookups or building active-tab UI (or assert with `CHECK` when an active tab is
a required precondition of the entry point). Do not rely solely on boolean
helpers querying active-tab properties (such as whether the active tab is in a
group), as they return `false` when no active tab exists and can cause branching
to falsely assume an active tab is present.

**Why.** When a `WebStateList` is empty or has no active selection,
`GetActiveWebState()` returns `nullptr` and `active_index()` returns
`WebStateList::kInvalidIndex`. Dereferencing unchecked pointers or passing
`kInvalidIndex` to index-based queries triggers fatal crashes, while boolean
property helpers returning `false` for absent tabs cause `if`/`else` branches to
inappropriately execute fallback active-tab actions.

## Examples

Bad:

```cpp
// 1. Dereferencing `GetActiveWebState()` without checking for `nullptr`:
web::WebStateID tab_id = browser_->GetWebStateList()
                             ->GetActiveWebState()
                             ->GetUniqueIdentifier();

// 2. Relying on boolean active-tab helper without checking `kInvalidIndex`:
if ([self activeWebStateInGroup]) {
  [self showMoveTabGroupMenu];
} else {
  // Wrongly executed when `active_index()` is `kInvalidIndex` (no active tab):
  return [actionFactory menuToAddTabToGroupWithGroups:allGroups
                                         numberOfTabs:1
                                                block:actionBlock];
}
```

Good:

```cpp
// 1. Verify `GetActiveWebState()` is non-null before dereferencing:
if (web::WebState* active_web_state =
        browser_->GetWebStateList()->GetActiveWebState()) {
  web::WebStateID tab_id = active_web_state->GetUniqueIdentifier();
}

// 2. Guard against `kInvalidIndex` before querying group or building UI:
int activeIndex = self.currentWebStateList->active_index();
if (activeIndex == WebStateList::kInvalidIndex) {
  return nil;
}
const TabGroup* group =
    self.currentWebStateList->GetGroupOfWebStateAt(activeIndex);
if (group) {
  [self showMoveTabGroupMenu];
} else {
  return [actionFactory menuToAddTabToGroupWithGroups:allGroups
                                         numberOfTabs:1
                                                block:actionBlock];
}
```
