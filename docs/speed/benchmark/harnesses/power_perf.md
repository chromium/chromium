# Power Telemetry tests

[TOC]

## Overview

The Telemetry power benchmarks measure power indirectly by measuring the CPU time used by Chrome while it performs various tasks (a.k.a. user stories).

## List of power metrics

### `cpu_time_percentage_avg`
This metric measures the average number of cores that Chrome used over the duration of the trace.

This metric is enabled by adding `'cpuTimeMetric'` to the list of TBM2 metrics in the benchmark's Python class:

```python
options.SetTimelineBasedMetrics(['cpuTimeMetric', 'memoryMetric'])
```

Additionally, the `toplevel` trace category must be enabled for this metric to function correctly because it ensures that a trace span is active whenever Chrome is doing work:

```python
category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(filter_string='toplevel')
```

## List of power benchmarks

The primary power benchmarks are:

- **`system_health.common_desktop`**: A desktop-only benchmark in which each page focuses on a single, common way in which users use Chrome (e.g. browsing Facebook photos, shopping on Amazon, searching Google)
- **`system_health.common_mobile`**: A mobile-only benchmark that parallels `system_health.common_desktop`
- **`power.desktop`**: A desktop-only benchmark made up of two types of pages:
  - Pages focusing on a single, extremely simple behavior (e.g. a blinking cursor, a CSS blur animation)
  - Pages on which Chrome has exhibited pathological idle behavior in the past
- **`media.desktop`**: A desktop-only benchmark in which each page tests a particular media-related scenario (e.g. playing a 1080p, H264 video with sound)
- **`media.mobile`**: A mobile-only benchmark that parallels `media.desktop`

[This spreadsheet](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0) lists the owner for each benchmark.

## Adding new power test cases
To add a new test case to a power benchmark, contact the owner of the benchmark above that sounds like the best fit.

## Running the benchmarks locally
See [this page](https://github.com/catapult-project/catapult/blob/master/telemetry/docs/run_benchmarks_locally.md) for instructions on how to run the benchmarks locally.

## Seeing power benchmark results
Enter the platform, benchmark, and metric you care about on [this page](https://chromeperf.appspot.com/report) to see how the power metrics have moved over time.
