---
name: ios/prefer-base-time-over-ns-date
rubric: readability
severity: should_fix
---

**Rule.** Use Chromium time primitives (`base::Time`, `base::TimeTicks`,
`base::TimeDelta`) for timing and durations rather than `NSDate` and
`NSTimeInterval` (except short UI animation durations).

**Why.** Chromium time types prevent unit confusion via explicit helper
constructors (`base::Seconds()`, `base::Minutes()`) and handle system clock
shifts reliably. See `ios/best_practices.md` ("Measuring Time: Use base::Time,
base::TimeTicks, and base::TimeDelta").

## Examples

Bad:

```cpp
const NSTimeInterval kSyncIntervalInSeconds = 3600;
NSDate* startTime = [NSDate date];
```

Good:

```cpp
constexpr base::TimeDelta kSyncInterval = base::Hours(1);
base::TimeTicks startTime = base::TimeTicks::Now();
```
