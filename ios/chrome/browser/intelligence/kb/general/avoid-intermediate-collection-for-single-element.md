---
name: ios/avoid-intermediate-collection-for-single-element
description: When searching, selecting, or passing a single element, accept and track the item directly instead of allocating or accepting an intermediate container
rubric: readability
severity: nit
---

**Rule.** Do not allocate an intermediate collection (such as an
`NSMutableArray` or `std::vector`) or design functions to accept a container
when only a single element—such as the first, last, or matching item—is needed.
In search and iteration loops, track the target candidate in a local variable or
return early. When a helper function only operates on a single element, accept
the element directly (by value or const reference) to avoid forcing callers with
a single object to construct temporary containers like `{item}`.

**Why.** Allocating temporary containers and populating them merely to select,
hold, or pass a single element incurs unnecessary heap allocations, element
copying, and object-retention overhead. Operating directly on the element avoids
dynamic memory overhead, keeps function signatures minimal, and clarifies caller
intent.

## Examples

Bad:

```cpp
// Bad: Allocating an intermediate array to retrieve only the last element.
NSMutableArray<UIWindow*>* windows = [[NSMutableArray alloc] init];
for (UIWindowScene* windowScene in windowScenes) {
  [windows addObjectsFromArray:windowScene.windows];
}
return [windows lastObject];

// Bad: Function accepts a container when only one element is used, forcing temporary collection.
std::string GetSuffix(const std::vector<Resource>& resources);
reporting_info.suffix = GetSuffix({resource});
```

Good:

```cpp
// Good: Tracking the candidate directly without intermediate container allocations.
UIWindow* lastWindow = nil;
for (UIWindowScene* windowScene in windowScenes) {
  UIWindow* window = [windowScene.windows lastObject];
  if (window) {
    lastWindow = window;
  }
}
return lastWindow;

// Good: Function accepts the element directly; container callers pass container[0].
std::string GetSuffix(const Resource& resource);
reporting_info.suffix = GetSuffix(resource);
```
