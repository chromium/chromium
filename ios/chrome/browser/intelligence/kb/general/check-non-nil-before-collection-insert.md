---
name: ios/check-non-nil-before-collection-insert
description: When inserting objects into Foundation collections like NSMutableArray or NSMutableSet, verify that the object is non-nil to prevent NSInvalidArgumentException crashes
rubric: correctness
severity: should_fix
---

**Rule.** Always verify that an Objective-C object is non-`nil` before inserting
it into a Foundation collection (such as using `[NSMutableArray addObject:]`,
`insertObject:atIndex:`, or `[NSMutableSet addObject:]`), especially when
obtaining elements from conversion helpers or factory functions that can return
`nil`. Do not pass unchecked nullable objects or conversion expressions directly
into collection insertion methods. If a `nil` value violates an invariant rather
than representing a normal optional return value, also assert it with
`CHECK(obj, base::NotFatalUntil::M161)` before returning or skipping the
insertion.

**Why.** In Objective-C, passing `nil` to Foundation collection mutation methods
like `[NSMutableArray addObject:]` throws an unhandled
`NSInvalidArgumentException` at runtime, causing a fatal application crash.
Unlike standard Objective-C message sends which safely evaluate to a no-op on
`nil`, Foundation collections require all contained elements to be non-`nil`.

## Examples

Bad:

```cpp
if (canonicalCookie) {
  [cookiesCopy
      addObject:SystemCookieFromCanonicalCookie(*canonicalCookie)];
}
```

Good:

```cpp
if (canonicalCookie) {
  NSHTTPCookie* systemCookie =
      SystemCookieFromCanonicalCookie(*canonicalCookie);
  if (systemCookie) {
    [cookiesCopy addObject:systemCookie];
  }
}
```
