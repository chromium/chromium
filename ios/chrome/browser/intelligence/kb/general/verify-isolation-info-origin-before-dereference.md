---
name: ios/verify-isolation-info-origin-before-dereference
description: Do not unconditionally dereference a std::optional<T> returned by an accessor (such as net::IsolationInfo::top_frame_origin() or frame_origin()) with operator-> or operator* without checking has_value() or value_or()
rubric: correctness
severity: should_fix
---

**Rule.** Do not unconditionally dereference a `std::optional<T>` returned by a
query or accessor (such as `net::IsolationInfo::top_frame_origin()` or
`frame_origin()`) with `operator->` or `operator*` without verifying that a
value is present. Check `.has_value()` or use `.value_or(...)` to handle empty
states gracefully (or `CHECK` when engagement is a required invariant),
especially when context objects can be default-constructed or unpopulated for
background, internal, or non-frame requests.

**Why.** Calling `operator->` or `operator*` on an empty `std::optional` invokes
undefined behavior and causes fatal runtime crashes. Queries and context
accessors (such as `top_frame_origin()` and `frame_origin()` on
`net::IsolationInfo`) routinely return `std::nullopt` in system, internal, or
non-navigation contexts.

## Examples

Bad:

```cpp
// Bad: Unconditionally dereferencing `top_frame_origin()` crashes when
// `IsolationInfo` is empty.
bool CheckRequest(const net::URLRequest& request) {
  return settings_->IsAllowed(
      request.url(),
      request.isolation_info().top_frame_origin()->GetURL());
}
```

Good:

```cpp
// Good: Verify the optional is engaged before dereferencing.
bool CheckRequest(const net::URLRequest& request) {
  const auto& top_frame_origin = request.isolation_info().top_frame_origin();
  if (!top_frame_origin.has_value()) {
    return false;
  }
  return settings_->IsAllowed(request.url(), top_frame_origin->GetURL());
}
```
