---
name: ios/hoist-loop-invariant-operations
description: When executing loops or polling condition blocks like WaitUntilConditionOrTimeout, hoist loop-invariant lookups, conditions, and state updates outside rather than re-evaluating or re-setting them per iteration
rubric: readability
severity: nit
---

**Rule.** Hoist loop-invariant operations, object or manager lookups, timestamp
queries, and state updates (such as calling property setters like
`setEditingAllowed:`, `SetEnabled`, or `SetVisible`) outside loops. Do not
repeatedly re-evaluate invariant conditions, re-query invariant services, or
invoke redundant setters inside an iteration loop when the state does not depend
on the iterated elements.

**Why.** Evaluating invariant operations, re-querying managers, or invoking
redundant setters on every loop iteration wastes CPU cycles, repeatedly
overwrites identical values, and clutters the loop body. It also misleads
readers into believing that the property or state depends on individual loop
elements. Hoisting invariant logic and lookups ensures state is cleanly
initialized once—even if the collection is empty—and keeps the loop body focused
strictly on dynamic per-element processing.

## Examples

Bad:

```cpp
// Bad: Repeatedly querying clock inside observer loop.
for (auto& observer : observer_list_) {
  idle_trigger_time_ = base::Time::Now();
  observer.OnIdleTimeoutInForeground();
}

// Bad: Re-evaluating invariant condition and calling setter per iteration.
[consumer setEditingAllowed:!_entityInstance->are_attributes_read_only()];
for (const auto& attribute : _entityInstance->attributes()) {
  if (_entityInstance->are_attributes_read_only()) {
    [consumer setEditingAllowed:NO];
  } else {
    [consumer setEditingAllowed:YES];
  }
  ProcessAttribute(attribute);
}
```

Good:

```cpp
// Good: Hoist timestamp query before the loop.
idle_trigger_time_ = base::Time::Now();
for (auto& observer : observer_list_) {
  observer.OnIdleTimeoutInForeground();
}

// Good: Set invariant state once outside the loop.
[consumer setEditingAllowed:!_entityInstance->are_attributes_read_only()];
for (const auto& attribute : _entityInstance->attributes()) {
  ProcessAttribute(attribute);
}
```
