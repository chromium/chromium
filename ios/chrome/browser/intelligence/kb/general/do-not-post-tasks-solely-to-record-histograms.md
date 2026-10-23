---
name: ios/do-not-post-tasks-solely-to-record-histograms
description: When recording UMA metrics in background tasks or asynchronous callbacks, invoke base::UmaHistogram* directly instead of posting tasks solely for telemetry emission
rubric: readability
severity: nit
---

**Rule.** Do not post a task to a `base::SequencedTaskRunner`,
`base::SingleThreadTaskRunner`, or background thread solely to record a UMA
histogram or user metric. Chromium's histogram recording APIs
(`base::UmaHistogram*` functions and `UMA_HISTOGRAM_*` macros) are thread-safe
and can be called directly from any sequence or worker thread. Record metrics
directly on the current thread at the event or callback site instead of
dispatching an extra task.

**Why.** Posting a task to another sequence solely to record telemetry
introduces unnecessary task scheduling, closure allocations, and thread hops.
Because UMA histogram APIs already handle thread safety internally (via locks or
atomic updates), recording directly on the current thread avoids thread
synchronization overhead without any risk of data races.

## Examples

Bad:

```cpp
// Bad: Posting an asynchronous task to a task runner solely to log a histogram.
task_runner->PostTask(
    FROM_HERE,
    base::BindOnce(
        [](bool success_val) {
          base::UmaHistogramBoolean(
              "Commerce.PriceTracking.Untrack.Success",
              success_val);
        },
        success));

// Bad: Binding an unnecessary sequence hop solely for metric recording.
fetcher->Fetch(base::BindPostTask(
    ui_task_runner,
    base::BindOnce(&base::UmaHistogramCounts100, "Network.FetchCount")));
```

Good:

```cpp
// Good: Record the histogram directly on the current thread.
base::UmaHistogramBoolean("Commerce.PriceTracking.Untrack.Success", success);

// Good: Emit directly in the completion callback without re-posting.
fetcher->Fetch(base::BindOnce(&base::UmaHistogramCounts100,
                              "Network.FetchCount"));
```
