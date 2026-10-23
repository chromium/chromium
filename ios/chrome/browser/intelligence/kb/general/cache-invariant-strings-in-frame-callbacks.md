---
name: ios/cache-invariant-strings-in-frame-callbacks
description: When implementing per-frame display link or high-frequency callbacks, cache invariant string conversions like SysNSStringToUTF8 and NSStringFromClass in ivars instead of recomputing on every frame
rubric: safety
severity: nit
---

**Rule.** In high-frequency or per-frame callbacks (such as `CADisplayLink`
handlers, display tracing, or layout passes), do not perform repeated
Objective-C runtime inspection (e.g. `NSStringFromClass`) or heap string
conversions (e.g. `base::SysNSStringToUTF8`). Cache the converted C++ string or
C-string in an instance variable or memoized getter during initialization or on
first access instead of recomputing it on every tick.

**Why.** Per-frame callbacks like `CADisplayLink` execute at the display refresh
rate (60Hz or 120Hz). Invoking Objective-C runtime methods and converting
`NSString*` to UTF-8 on every invocation repeatedly allocates heap memory and
incurs CPU overhead on the main thread for values that remain invariant over the
object's lifetime.

## Examples

Bad:

```cpp
// Bad: Invoking runtime inspection and UTF-8 string conversion on every frame tick.
- (void)displayLinkDidFire:(CADisplayLink*)displayLink {
  TRACE_EVENT_INSTANT(
      "ui",
      perfetto::DynamicString(
          base::SysNSStringToUTF8(NSStringFromClass([self class]))),
      perfetto::NamedTrack("Display"));
}
```

Good:

```cpp
// Good: Cache the invariant string once during initialization in an ivar.
- (instancetype)init {
  if ((self = [super init])) {
    _className = base::SysNSStringToUTF8(NSStringFromClass([self class]));
  }
  return self;
}

- (void)displayLinkDidFire:(CADisplayLink*)displayLink {
  TRACE_EVENT_INSTANT("ui", perfetto::DynamicString(_className),
                      perfetto::NamedTrack("Display"));
}
```
