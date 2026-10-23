---
name: ios/avoid-weakptr-as-lazy-initialization-cache
description: When implementing lazy initialization or caching dependencies, do not use base::WeakPtr as an uninitialized sentinel; query directly or use an explicit boolean flag
rubric: correctness
severity: should_fix
citations:
  - path: base/memory/README.md
    heading: base::WeakPtr<T>
    quote: When the associated factory (usually owned by T) is destroyed, all `WeakPtr<T>` are invalidated (becomes null) rather than becoming use-after-frees.
---

**Rule.** Do not use `base::WeakPtr` as a lazy-initialization sentinel or cached
pointer check (such as `if (!weak_ptr)` to trigger initial acquisition). Because
a weak pointer automatically resets to null when its pointee is destroyed,
checking nullity cannot differentiate between an uninitialized state and an
object that was already destroyed. In accessors or getters, retrieve the
dependency directly from its owning context or service on each call, or use an
explicit initialization boolean flag if caching an optional or ephemeral
pointer.

**Why.** A `base::WeakPtr` automatically invalidates and clears to null upon
object destruction. Using `!weak_ptr` as an initialization sentinel causes
subsequent calls after pointee destruction (such as during shutdown, test
teardown, or window closing) to re-trigger the initialization block. This can
dereference null service pointers, cause use-after-free crashes, or resurrect
dependencies in an invalid state during application teardown.

## Examples

Bad:

```cpp
- (PushNotificationClientManager*)appWideClientManager {
  if (!_appWideClientManager) {
    _appWideClientManager = GetApplicationContext()
                                ->GetPushNotificationService()
                                ->GetPushNotificationClientManager()
                                ->AsWeakPtr();
  }
  return _appWideClientManager.get();
}
```

Good:

```cpp
- (PushNotificationClientManager*)appWideClientManager {
  return GetApplicationContext()
      ->GetPushNotificationService()
      ->GetPushNotificationClientManager();
}
```
