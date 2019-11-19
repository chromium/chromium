# How Chrome Measures Performance

Chrome collects performance data both in the lab and from end users. There are
thousands of individual metrics. This is an overview of how to sort through
them at a high level.

## Domain Areas

At a high level, performance work in Chrome is categorized into **domains**,
like loading, memory, and power. Each domain has critical laboratory and
end-user metrics associated with it.

  * **[An overview of domains](speed_domains.md)**: lists the domains and key contact points.
  * **[Speed Launch Metrics](https://docs.google.com/document/d/1Ww487ZskJ-xBmJGwPO-XPz_QcJvw-kSNffm0nPhVpj8/edit)**:
    the important high-level end-user metrics we measure for each domain.
  * TODO: Link to documentation on critical laboratory measurements for each
    domain.

## Laboratory Metrics

Chrome has multiple performance labs in which benchmarks are run on continuous
builds to pinpoint performance regressions down to individual changelists.

### The chrome.perf lab

The main lab for performance monitoring is
[chrome.perf](https://ci.chromium.org/p/chrome/g/chrome.perf/console). It continuously tests
chromium commits and is monitored by several perf sheriff rotations.

  * **[What is the perf waterfall?](perf_waterfall.md)** An overview of the
    waterfall that runs the continuous build.
  * **[How telemetry works](https://github.com/catapult-project/catapult/blob/master/telemetry/README.md)**:
    An overview of telemetry, our performance testing harness.
  * **[How perf bisects work](bisects.md)**: An overview of the bisect bots,
    which narrow down regressions over a CL range to a specific commit.
  * **Benchmarks**
    * **[Benchmark Policy](https://docs.google.com/document/d/1ni2MIeVnlH4bTj4yvEDMVNxgL73PqK_O9_NUm3NW3BA/edit)**:
      An overview of the benchmark harnesses available in Chrome, and how to
      find the right place to add a new test case.
    * **[System health benchmarks](https://docs.google.com/document/d/1BM_6lBrPzpMNMtcyi2NFKGIzmzIQ1oH3OlNG27kDGNU/edit?ts=57e92782)**:
      The system health benchmarks measure the speed launch metrics on
      real-world web use scenarios.
  * **[How to run on perf trybots](perf_trybots.md)**: Have an unsubmitted
    CL and want to run benchmarks on it? Need to try a variety of hardware and
    operating systems? Use the perf trybots.
  * **[How to run telemetry locally](https://github.com/catapult-project/catapult/blob/master/telemetry/docs/run_benchmarks_locally.md)**:
    Instructions on running telemetry benchmarks on your local machine.
  * **[List of platforms in the lab](perf_lab_platforms.md)**: Devices,
    configurations, and OSes the chromium.perf lab tests on.

### Other performance labs

There are several other performance labs for specialized use:

  * **[Lab Spotlight: AV Lab (Googlers only)](http://goto.google.com/av-analysis-service)**:
    Learn all about audio/video quality testing.
  * **[Lab Spotlight: Cluster telemetry](https://docs.google.com/document/d/1GhqosQcwsy6F-eBAmFn_ITDF7_Iv_rY9FhCKwAnk9qQ/edit)**:
    Need to run a performance test over thousands of pages? Check out cluster
    telemetry!

## End-user metrics

The **[Speed Launch Metrics](https://docs.google.com/document/d/1Ww487ZskJ-xBmJGwPO-XPz_QcJvw-kSNffm0nPhVpj8/edit)**
doc explains metrics available in UMA for end user performance. If you want to
test how your change impacts these metrics for end users, you'll probably want
to **[Run a Finch Trial](http://goto.google.com/finch101)**.

The **[UMA Sampling Profiler (Googlers only)](http://goto.google.com/uma-sampling-profiler-overview)**
measures Chrome execution using statistical profiling, producing aggregate
execution profiles across the function call tree. The profiler is useful for
understanding how your code performs for end users, and the precise performance
impact of code changes.
