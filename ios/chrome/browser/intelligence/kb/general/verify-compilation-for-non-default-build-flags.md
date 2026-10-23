---
name: ios/verify-compilation-for-non-default-build-flags
description: When modifying code guarded by non-default build flags or internal configurations, verify compilation via specialized trybots or temporary GN flag overrides before landing
rubric: correctness
severity: should_fix
citations:
  - path: docs/contributing.md
    heading: 'Cq-Include-Trybots:'
    quote: A comma-separated list of trybots which should be triggered and checked by the CQ in addition to the normal set.
---

**Rule.** When refactoring APIs or modifying code, source sets, or dependencies
referenced inside `#if BUILDFLAG(...)` blocks guarded by non-default GN build
flags, update all call sites inside the `#if BUILDFLAG(...)` blocks and verify
compilation before landing. Trigger specialized trybots (using the Gerrit
"Choose Tryjobs" UI or the `Cq-Include-Trybots:` commit footer) or temporarily
enable the GN build argument in a draft patch set to confirm that the
conditional code builds cleanly.

**Why.** Default CQ trybot runs only build and test standard build flag
configurations. Code gated behind non-default `#if BUILDFLAG(...)` guards is
omitted from standard CQ compilation, allowing stale signatures, type
mismatches, and missing includes to pass default trybots unnoticed and break
specialized builders upon landing.

## Examples

Bad:

```cpp
// Bad: Updating a method signature without updating its #if BUILDFLAG(...) call site
// or verifying non-default build flag compilation:
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
if (start_on_device) {
  StartOnDeviceSession();  // Stale: the method now takes a `config`.
}
#endif
// Landing without updating the guarded call or running specialized trybots.
```

Good:

```cpp
// Good: Update code inside #if BUILDFLAG(...) blocks and verify compilation via
// Cq-Include-Trybots or a draft patch set with the flag enabled:
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
if (start_on_device) {
  StartOnDeviceSession(config);
}
#endif
// Commit footer verified on builder before landing:
// Cq-Include-Trybots: luci.chromium.try:ios-simulator-full-configs
```
