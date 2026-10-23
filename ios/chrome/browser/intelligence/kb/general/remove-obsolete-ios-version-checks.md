---
name: ios/remove-obsolete-ios-version-checks
description: When writing iOS code at or below the minimum deployment target, omit redundant @available, API_AVAILABLE, and IsRunningOnIOS checks, except in shared embedder libraries
rubric: correctness
severity: should_fix
citations:
  - path: docs/supported_platforms.md
    heading: iOS
    quote: The minimum supported operating system version changes roughly once a year.
---

**Rule.** In Chrome-specific iOS code and tests (`ios/chrome/...`), do not
retain or add runtime `@available(...)` or `__builtin_available(...)` guards,
`base::ios::IsRunningOnIOS*OrLater()` helper checks, dead fallback branches,
early returns, or redundant `API_AVAILABLE(ios(...))` annotations targeting iOS
versions at or below the project's minimum deployment target
(`ios_deployment_target` / `IPHONEOS_DEPLOYMENT_TARGET`). Invoke APIs and modern
behavior directly and unconditionally. Conversely, in shared iOS libraries or
components (such as `//ios/net` or `//ios/web`), do not remove `@available(...)`
checks or fallback paths without verifying whether external embedders (such as
Cronet or Web View) build with an earlier deployment target than Chrome.

**Why.** APIs and symbols introduced at or below the minimum deployment target
are unconditionally available at compile time and runtime across all supported
Chrome devices and build configurations. Adding `@available`, `API_AVAILABLE`,
or `base::ios::IsRunningOnIOS*OrLater` checks for baseline versions introduces
unnecessary boilerplate, dead runtime fallback branches, and maintenance
overhead. However, shared libraries (`//ios/net`, `//ios/web`) are consumed by
external embedders (such as Cronet) that may support lower deployment targets
than Chrome; removing version checks in shared code breaks compatibility or
compilation for those embedders.

## Examples

Bad:

```cpp
// Bad: Redundant API_AVAILABLE annotation on declaration (iOS baseline version).
extern NSString* const kBaselineSymbol API_AVAILABLE(ios(18.0));

// Bad: Redundant @available check and dead fallback branch in Chrome code.
if (@available(iOS 18.0, *)) {
  [snapshot reconfigureItemsWithIdentifiers:items];
} else {
  [snapshot reloadItemsWithIdentifiers:items];
}

// Bad: Prematurely removing @available in shared library (//ios/net) used by Cronet.
void CheckSystemCookie(...) {
  EXPECT_NSEQ(NSHTTPCookieSameSiteLax, [system_cookie sameSitePolicy]);
}
```

Good:

```cpp
// Good: Direct symbol declaration without redundant availability annotation.
extern NSString* const kBaselineSymbol;

// Good: Call modern API directly without redundant @available check or fallback.
[snapshot reconfigureItemsWithIdentifiers:items];

// Good: Retain @available checks in shared embedder libraries with lower baselines.
void CheckSystemCookie(...) {
  if (@available(iOS 14.0, *)) {
    EXPECT_NSEQ(NSHTTPCookieSameSiteLax, [system_cookie sameSitePolicy]);
  }
}
```
