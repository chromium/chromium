---
name: net/device_bound_sessions/scope-request-path-histograms-to-dbsc-requests
rubric: perf
severity: nit
---

**Rule.** Prefer recording `Net.DeviceBoundSessions.*` histograms from
`URLRequestHttpJob` and other per-request code only for requests in scope of a
DBSC session, for example behind `device_bound_sessions::IsInScope()`. This is
guidance rather than a hard requirement: a histogram whose denominator has to be
every request, such as `Net.DeviceBoundSessions.RequestDeferralDecision3`, which
measures how many requests DBSC touches at all, can record per request when the
CL explains why.

**Why.** `URLRequestHttpJob` runs for every network request, so a per-request
histogram adds work on a hot path, and most of those requests never involve
DBSC, which dilutes the metric. Scoping the recording to DBSC requests keeps the
cost proportional to DBSC usage and the data about what DBSC actually does.

## Examples

Bad:

```cpp
// Records for every HTTP request, although most have no DBSC session in scope,
// and nothing explains why they belong in the denominator.
base::UmaHistogramBoolean("Net.DeviceBoundSessions.RequestWasDeferred",
                          was_deferred);
```

Good:

```cpp
if (device_bound_sessions::IsInScope(max_usage)) {
  base::UmaHistogramBoolean(
      "Net.DeviceBoundSessions.InScopeRequestWasDeferred", was_deferred);
}
```
