---
name: ios/avoid-repurposing-properties-with-different-semantics
description: When storing or exposing data with distinct semantics (such as navigation timestamps), do not repurpose existing properties like last_active_time; introduce separate accessors
rubric: design
severity: should_fix
citations:
  - path: docs/ios/unrealized_web_state.md
    heading: Unrealized WebState
    quote: When in the unrealized state, a WebState will not be connected to any WebView. In fact, they will only store their identifier, their creation time, their last active time, the title and URL of the visible page, and the number of navigation item.
  - path: ios/web/README.md
    heading: Overview
    quote: ios/web performs a similar role on iOS as what content/ does on other platforms.
---

**Rule.** Do not repurpose an existing property, getter, or serialized field to
store or expose data with different semantics (for example, storing a navigation
timestamp in a field defined as `last_active_time`). When new or alternative
semantic data is needed, introduce a dedicated, distinctly named property,
accessor, or field instead of overriding existing field values.

**Why.** Repurposing an existing property or field to represent different data
creates misleading APIs, produces semantic drift, and breaks callers and
observers that rely on the original contract (such as Sync or tab-inactivity
heuristics that distinguish user activation time from navigation commit time).

## Examples

Bad:

```cpp
// Bad: Overwriting last_active_time with navigation commit timestamp.
void UpdateTabMetadata(FooTabMetadata& metadata) {
  metadata.last_active_time = last_active_time_;
  if (NavigationItem* item = navigation_manager_->GetLastCommittedItem()) {
    metadata.last_active_time = item->GetTimestamp();
  }
}
```

Good:

```cpp
// Good: Preserve last_active_time semantics and use a dedicated field for
// navigation timestamps.
void UpdateTabMetadata(FooTabMetadata& metadata) {
  metadata.last_active_time = last_active_time_;
  if (NavigationItem* item = navigation_manager_->GetLastCommittedItem()) {
    metadata.last_navigation_time = item->GetTimestamp();
  }
}
```
