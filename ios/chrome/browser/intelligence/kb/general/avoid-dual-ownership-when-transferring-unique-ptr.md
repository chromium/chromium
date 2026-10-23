---
name: ios/avoid-dual-ownership-when-transferring-unique-ptr
description: When transferring ownership of an object held in std::unique_ptr, use std::move rather than constructing a new std::unique_ptr or base::WrapUnique from .get()
rubric: safety
severity: blocking
citations:
  - path: styleguide/c++/c++.md
    heading: Object ownership and calling conventions
    quote: If the function takes ownership of a non-refcounted object, declare the param as `std::unique_ptr<T>`.
---

**Rule.** When transferring ownership of an object managed by a
`std::unique_ptr`, transfer ownership using `std::move()` rather than
constructing a new `std::unique_ptr` or calling `base::WrapUnique()` on a raw
pointer obtained via `.get()`. If the object must be accessed or inspected after
ownership is transferred, retain a non-owning raw pointer (`.get()`) beforehand,
but still transfer the managing `std::unique_ptr` via `std::move()`.

**Why.** Constructing a new `std::unique_ptr` or calling `base::WrapUnique()` on
a raw pointer extracted from an existing smart pointer creates duplicate owning
instances for the same heap allocation without releasing the original owner.
When either smart pointer is destroyed or reset, the pointee is deleted,
inevitably leading to double-free crashes or use-after-free vulnerabilities.

## Examples

Bad:

```cpp
// Bad: re-wrapping .get() creates dual ownership and a double-free crash.
NavigationItem* item = pending_item_.get();
SetNavigationItemInWKItem(
    current_wk_item, std::unique_ptr<NavigationItem>(item));

// Bad: wrapping .get() in base::WrapUnique leaves pending_item_ as duplicate owner.
SetNavigationItemInWKItem(
    current_wk_item, base::WrapUnique(pending_item_.get()));
```

Good:

```cpp
// Good: retain non-owning raw pointer beforehand if needed, and transfer via std::move().
NavigationItem* item = pending_item_.get();
SetNavigationItemInWKItem(current_wk_item, std::move(pending_item_));

// Safe: access via the retained non-owning raw pointer while the new owner keeps it alive.
item->SetTransitionType(transition);
```
