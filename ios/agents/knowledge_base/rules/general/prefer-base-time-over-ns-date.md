---
name: ios/prefer-base-time-over-ns-date
description: Use base::Time, base::TimeTicks, and base::TimeDelta rather than NSDate, NSTimeInterval, or raw integer duration constants, except for short UIKit animation durations and Apple API boundaries
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: 'Measuring Time: Use `base::Time`, `base::TimeTicks`, and `base::TimeDelta`'
    quote: Use **`base::TimeDelta`** constants and constructors (`base::Seconds(5)`, `base::Minutes(10)`, `base::Days(7)`) instead of raw `NSTimeInterval` or integer constants (which risk unit mismatches at the call site)
---

**Rule.** Use `base::TimeDelta` (`base::Seconds()`, `base::Minutes()`) for
durations rather than `NSTimeInterval` or raw integer constants. Use
`base::TimeTicks::Now()` for monotonic elapsed durations and `base::Time::Now()`
for wall-clock timestamps rather than `NSDate`. `NSTimeInterval` and `NSDate`
are permitted for short UIKit animation durations and at Apple API boundaries
that require them (converting via `base::Time::FromNSDate()` and `ToNSDate()`).

**Why.** `base::TimeDelta` encodes units in the type to prevent call-site unit
mismatches. Separating monotonic `base::TimeTicks` from wall-clock `base::Time`
avoids clock-adjustment bugs when measuring elapsed durations. See
`ios/best_practices.md#measuring-time-use-basetime-basetimeticks-and-basetimedelta`.

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
