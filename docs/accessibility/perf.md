# Accessibility Performance Measurements

Accessibility support in Chromium can be expensive, so we have some
performance tests to help catch regressions and encourage
improving performance over time.

As background material, read up on
[Telemetry](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md),
Chromium's performance testing framework.

The metrics for accessibility are defined here:

```third_party/catapult/tracing/tracing/metrics/accessibility_metric.html```

They measure the time spent in three key parts of accessibility code
in Chrome, wherever you see SCOPED_TRACE. If there's any nontrivial
code that you think is being called that isn't covered now, the first
step would be to add a new SCOPED_TRACE, then update this metric to
start tracking it.

## Stories

The stories are defined here:

```tools/perf/page_sets/system_health/accessibility_stories.py```

A story is a url to load and other actions to take on that page. The
accessibility stories are also configured to run with accessibility
enabled via the command-line flag.

If there's a particular web page that seems slow and you'd like to
start tracking its performance, the first step would be to try adding
a new story here.

Here's an example command line to run all of the accessibility stories
locally using the currently installed Canary browser:

```tools/perf/run_benchmark system_health.common_desktop --story-filter="accessibility.*" --browser canary```

See the [documentation](https://github.com/catapult-project/catapult/blob/master/telemetry/docs/run_benchmarks_locally.md)
or command-line help for tools/perf/run_benchmark for
more command-line arguments.

Here's an example command line to run to capture the web page replay for
all accessibility stories, if you add a new story or update an existing one:

```tools/perf/record_wpr desktop_system_health_story_set --story-filter="accessibility.*" --browser canary```

Running this will upload the web page replay data captured and modify this
file, which should be submitted as part of your changelist:

```tools/perf/page_sets/data/system_health_desktop.json```

For more information, see [Record a page set](https://sites.google.com/a/chromium.org/dev/developers/telemetry/record_a_page_set).

## Blink Perf

In addition, we have Blink perf tests here. These are microbenchmarks
intended to track the performance of a small function or operation
in isolation. You can find these tests here:

```third_party/blink/perf_tests/accessibility/```

## Results

The results can be found at
[https://chromeperf.appspot.com](chromeperf.appspot.com).
Because that site displays graphs, we also maintain a command-line
script (Google-internal only) as a more accessible alternative way to
examine the same data via the command line.

To get the script:

```git clone sso://user/dmazzoni/chrome-accessibility-site``` and then run:

```./accessibility_issues.py perf```
