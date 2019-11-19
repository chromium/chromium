# Accessibility Performance Measurements

Accessibility support can have a negative impact on performance, so it is
important to test for regressions and to improve performance over time. This can
be done with Chromium's performance testing framework,
[Telemetry](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md).

The metrics for accessibility are defined in
[third_party/catapult/tracing/tracing/metrics/accessibility_metric.html](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/accessibility_metric.html).

## Stories

A story is a url to load and actions to take on that page.

Accessibility stories are defined in
[tools/perf/page_sets/system_health/accessibility_stories.py](https://cs.chromium.org/chromium/src/tools/perf/page_sets/system_health/accessibility_stories.py).
If a particular web page seems slow, you may add a new story here to track its
performance.

### Running stories from the command line

To run all accessibility stories locally using the currently installed Canary
browser:

```shell
tools/perf/run_benchmark system_health.common_desktop \
  --story-filter="accessibility.*" \
  --browser canary
```

To run the same set of tests on your own compiled version of Chrome:

```shell
tools/perf/run_benchmark system_health.common_desktop \
  --story-filter="accessibility.*" \
  --browser=exact \
  --browser-executable=out/Release/chrome
```

For more information, see the
[Telemetry documentation](https://github.com/catapult-project/catapult/blob/master/telemetry/docs/run_benchmarks_locally.md)
or command-line help for tools/perf/run_benchmark.

### Capture web page replay for new or modified stories

You should capture web page replay information whenever you add or modify a
story. The following command captures the web page replay for all accessibility
stories:

```shell
tools/perf/record_wpr desktop_system_health_story_set \
  --story-filter="accessibility.*" \
  --browser canary
```

Running this will upload the web page replay data captured and modify
[tools/perf/page_sets/data/system_health_desktop.json](https://cs.chromium.org/chromium/src/tools/perf/page_sets/data/system_health_desktop.json),
which should be submitted as part of your changelist. For more information, see
[Record a page set](https://sites.google.com/a/chromium.org/dev/developers/telemetry/record_a_page_set).

## Blink Perf

Blink perf tests are microbenchmarks that track the performance of a small
function or operation in isolation. You can find these tests in
[third_party/blink/perf_tests/accessibility/](https://cs.chromium.org/chromium/src/third_party/blink/perf_tests/accessibility/).

To run these tests locally on your own compiled Chrome:

```shell
tools/perf/run_benchmark blink_perf.accessibility \
  --browser=exact \
  --browser-executable=out/Release/chrome
```

## Results

The results can be found at
[https://chromeperf.appspot.com](chromeperf.appspot.com).

## Google-internal Info

More information on analytics and performance, including some additional
Google-internal resources, can be found at
[go/chrome-a11y/resources/analytics-performance](http://go/chrome-a11y/resources/analytics-performance).
