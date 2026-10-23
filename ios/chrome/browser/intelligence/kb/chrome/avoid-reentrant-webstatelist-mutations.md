---
name: ios/avoid-reentrant-webstatelist-mutations
description: When handling WebStateListObserver callbacks or tab gestures, avoid re-entrant WebStateList mutations and do not revert tabs using cached integer indices
rubric: correctness
severity: blocking
citations:
  - path: docs/ios/objects.md
    heading: WebStateList
    quote: WebStateList represents a list of WebStates. It maintains a notion of an active WebState and opener-opened relationship between WebStates. The WebStateList exposes an API to observe modification to the list of WebStates (additions, moves, removals, replacements).
---

**Rule.** Do not mutate `WebStateList` (such as activating, inserting, moving,
replacing, or closing tabs) directly or indirectly from within
`WebStateListObserver` or `WebStateListObserving` callbacks or synchronous side
effects triggered by them (such as synchronous `UIGestureRecognizer`
cancellation).

**Why.** `WebStateList` enforces non-reentrancy during observer notifications
via `LockForMutation()`, failing with a `CHECK(!locked_)` crash if an observer
or synchronous side effect triggers a nested mutation on the same
`WebStateList`.

## Examples

Bad:

```cpp
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // Crashes with CHECK(!locked_) because WebStateList is locked during observer notification:
  webStateList->ActivateWebStateAt(0);
}
```

Good:

```cpp
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // Update local consumer state without mutating WebStateList reentrantly:
  web::WebState* activeWebState = webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }
  [self.consumer
      setActiveWebStateIdentifier:activeWebState->GetUniqueIdentifier()];
}
```
