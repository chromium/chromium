# Chrome Speed Operations

Chrome Speed Operations provides tools, framework, and infrastructure
to track Chrome's performance and guide optimizations to Chrome.

TPM: ushesh@chromium.org

Speed Operations consists of 3 teams, working in tandem:

## Benchmarking
The Benchmarking team provides:
  * A set of [opinionated benchmarking frameworks](https://docs.google.com/document/d/1ni2MIeVnlH4bTj4yvEDMVNxgL73PqK_O9_NUm3NW3BA/edit)
    that make it easy for Chromium developers to add benchmarks for
    the areas of Chrome performance important to them.
  * A [perf waterfall](perf_waterfall.md) to run these benchmarks on our continuous build on a dozen
    real device types, on Windows, Mac, Linux, and Android.

TL: johnchen@chromium.org

## Speed Tooling
The [Speed Tooling](chrome_speed_tooling.md) team provides:
  * The [Chrome performance dashboard](https://chromeperf.appspot.com), which
    stores performance timeseries and related debugging data. The dashboard
    automatically detects regressions in these timeseries and has integration
    with Chromium's bug tracker for easy tracking.
  * Tools for [bisecting regressions](bisects.md) on our continuous build down
    to an exact culprit CL.
  * A [performance try job service](perf_trybots.md) which allows chromium
    developers to run benchmarks on unsubmitted CLs using the same hardware
    we use in the continuous build.

TL: dberris@chromium.org

## Releasing and System Health
The releasing team provides:
  * Tracking of all performance regressions seen in user-visible performance
    both in the wild and in the lab.
  * A per-milestone report on Chrome's performance.
  * Recommendations about lab hardware and new test scenarios.
  * Management of performance sheriff rotations.

TPM: ushesh@chromium.org
