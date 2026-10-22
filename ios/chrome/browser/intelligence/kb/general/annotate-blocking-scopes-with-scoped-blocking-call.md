---
name: ios/annotate-blocking-scopes-with-scoped-blocking-call
rubric: safety
severity: should_fix
---

**Rule.** Annotate scopes that perform synchronous or blocking OS operations
(such as Keychain queries, synchronous file system calls, or third-party
blocking library calls) outside of `//base` by instantiating
`base::ScopedBlockingCall` with `base::BlockingType::MAY_BLOCK` or
`base::BlockingType::WILL_BLOCK`. Restrict blocking operations to worker threads
and thread pools—never run them on the main/UI or IO threads—and keep
CPU-intensive computation outside the blocking scope. Blocking `//base` APIs do
not require outer annotations as they instantiate their own `ScopedBlockingCall`
internally.

**Why.** Synchronous blocking calls on UI or IO threads cause responsiveness
jank and trigger OS watchdog terminations. In background worker pools,
un-annotated blocking calls starve other tasks by consuming worker threads
without signaling the scheduler; `base::ScopedBlockingCall` asserts
`base::AssertBlockingAllowed()` in debug builds to catch thread constraint
violations early and allows the thread pool to expand capacity while the thread
waits off-CPU.

## Examples

Bad:

```cpp
// Bad: Calling synchronous OS/Keychain operations without `ScopedBlockingCall`.
std::optional<std::string> GetAccessGroup() {
  // Performs blocking `SecItem` queries.
  auto app_id = ComputeAppIdentifierPrefix();
  if (!app_id.has_value()) {
    return std::nullopt;
  }
  return base::StrCat({*app_id, ".bundle.id"});
}
```

Good:

```cpp
// Good: Annotate the blocking scope to assert thread restrictions and assist
// the scheduler.
std::optional<std::string> GetAccessGroup() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  // Performs blocking `SecItem` queries.
  auto app_id = ComputeAppIdentifierPrefix();
  if (!app_id.has_value()) {
    return std::nullopt;
  }
  return base::StrCat({*app_id, ".bundle.id"});
}
```
