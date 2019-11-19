# Memory Benchmarks

This document describes benchmarks available to track Chrome's and
WebView's memory usage, where they live, what they measure, how to run them,
and on how to diagnose regressions.

[TOC]

## Glossary

*   **User story:** a set of actions to perform on a browser or device (e.g.
    open google homepage, type "foo", click search, scroll down, visit first
    result, etc.).
*   **Metric:** a data aggregation process that takes a Chrome trace as input
    (produced by a [Telemetry][] run) and produces a set of summary numbers as
    output (e.g. total GPU memory used).
*   **Benchmark:** a combination of (one or more) user stories and (one or
    more) metrics.

[Telemetry]: https://github.com/catapult-project/catapult/blob/master/telemetry/README.md

## System Health

*System health* is an effort to unify top-level benchmarks (as opposite to
micro-benchmarks and regression tests) that are suitable to capture
representative user stories.

### Benchmarks

System health memory benchmarks are:

*   [system_health.memory_mobile][system_health] -
    user stories running on Android devices.
*   [system_health.memory_desktop][system_health] -
    user stories running on desktop platforms.

These benchmarks are run continuously on the [chrome.perf][] waterfall,
collecting and reporting results on the
[Chrome Performance Dashboard][chromeperf].

[system_health]: https://chromium.googlesource.com/chromium/src/+/master/tools/perf/page_sets/system_health/
[chrome.perf]: https://ci.chromium.org/p/chrome/g/chrome.perf/console
[chromeperf]: https://chromeperf.appspot.com/report

### User stories

System health user stories are classified by the kind of interactions they
perform with the browser:

*   `browse` stories navigate to a URL and interact with the page; e.g.
    scroll, click on elements, navigate to subpages, navigate back.
*   `load` stories just navigate to a URL and wait for the page to
    load.
*   `background` stories navigate to a URL, possibly interact with the
    page, and then bring another app to the foreground (thus pushing the
    browser to the background).
*   `long_running` stories interact with a page for a longer period
    of time (~5 mins).
*   `blank` has a single story that just navigates to **about:blank**.

The full name of a story has the form `{interaction}:{category}:{site}` where:

*   `interaction` is one the labels given above;
*   `category` is used to group together sites with a similar purpose,
    e.g. `news`, `social`, `tools`;
*   `site` is a short name identifying the website in which the story mostly
    takes place, e.g. `cnn`, `facebook`, `gmail`.

For example `browse:news:cnn` and `background:social:facebook` are two system
health user stories.

Today, for most stories a garbage collection is forced at the end of the
story and a memory dump is then triggered. Metrics report the values
obtained from this single measurement.

## Continuous monitoring

![Chrome Performance Dashboard](https://storage.googleapis.com/chromium-docs.appspot.com/79d08f59cf497c761f7099ea427704c14e9afc03.png)

To view data from one of the benchmarks on the
[Chrome Performance Dashboard][chromeperf] you should select:

*   **Test suite:** The name of a *[benchmark](#Benchmarks)*.
*   **Bot:** The name of a *platform or device configuration*. Sign in to also
    see internal bots.
*   **Subtest (1):** The name of a *[metric](#Understanding-memory-metrics)*.
*   **Subtest (2):** The name of a *story group*; these have the form
    `{interaction}_{category}` for system health stories.
*   **Subtest (3):** The name of a *[user story](#User-stories)*
    (with `:` replaced by `_`).

If you are investigating a Perf dashboard alert and would like to see the
details, you can click on any point of the graph. It gives you the commit range,
buildbot output and a link to the trace file taken during the buildbot run.
(More information about reading trace files [here][memory-infra])

[memory-infra]: /docs/memory-infra/README.md

![Chrome Performance Dashboard Alert](https://storage.googleapis.com/chromium-docs.appspot.com/perfdashboard_alert.png)

## How to run the benchmarks

Benchmarks may be run on a local platform/device or remotely on a pinpoint
try job.

### How to run a pinpoint try job

Given a patch already uploaded to code review, try jobs provide a convenient
way to evaluate its memory implications on devices or platforms which
may not be immediately available to developers.

![New pinpoint try job dialog](https://storage.googleapis.com/chromium-docs.appspot.com/yHRMmUqraqJ.png)

To start a try job go to the [pinpoint][] website, click on the `+` button to
create a new job, and fill in the required details:

[pinpoint]: https://pinpoint-dot-chromeperf.appspot.com/

* **Bug ID** (optional): The id of a crbug.com issue where pinpoint can post
  updates when the job finishes.
* **Gerrit URL**: URL to the patch you want to test. Note that your patch can
  live in chromium or any of its sub-repositories!
* **Bot**: Select a suitable device/platform from the drop-down menu on which
  to run your job.
* **Benchmark**: The name of the benchmark to run. If you are interested in
  memory try `system_health.memory_mobile` or `system_health.memory_desktop`
  as appropriate.
* **Story** (optional): A pattern passed to Telemetry's `--story-filter`
  option to only run stories that match the pattern.
* **Extra Test Arguments** (optional): Additional command line arguments for
  Telemetry's `run_benchmark`. Of note, if you are interested in running a
  small but representative sample of system health stories you can pass
  `--story-tag-filter health_check`.

If you have more specific needs, or need to automate the creation of jobs, you
can also consider using [pinpoint_cli][].

[pinpoint_cli]: https://cs.chromium.org/chromium/src/tools/perf/pinpoint_cli

### How to run locally

After building, e.g. `ChromePublic.apk`, you can run a specific system health
story with the command:

```
$SRC/tools/perf/run_benchmark run system_health.memory_mobile \
    --browser android-chromium --story-filter load:search:google
```

This will run the story with a default of 3 repetitions and produce a
`results.html` file comparing results from this and any previous benchmark
runs. In addition, you'll also get individual [trace files][memory-infra]
for each story run by the benchmark. **Note:** by default only high level
metrics are shown, you may need to tick the "Show all" check box in order to
view some of the lower level memory metrics.

![Example results.html file](https://storage.googleapis.com/chromium-docs.appspot.com/ea60207d9bb4809178fe75923d6d1a2b241170ef.png)

Other useful options for this command are:

*   `--pageset-repeat [n]` - override the default number of repetitions
*   `--reset-results` - clear results from any previous benchmark runs in the
    `results.html` file.
*   `--results-label [label]` - give meaningful names to your benchmark runs,
    this way it is easier to compare them.

For WebView make sure to [replace the system WebView][webview_install]
on your device and use `--browser android-webview`.

[memory-infra]: /docs/memory-infra/README.md
[webview_install]: https://www.chromium.org/developers/how-tos/build-instructions-android-webview

## Understanding memory metrics

There is a large number of [memory-infra][] metrics, breaking down usage
attributed to different components and processes.

![memory-infra metrics](https://storage.googleapis.com/chromium-docs.appspot.com/a73239c6367ed0f844500e51ce1e04556cb99b4f.png)

Most memory metrics have the form
`memory:{browser}:{processes}:{source}:{component}:{kind}`
where:

*   **browser:** One of `chrome` or `webview`.
*   **processess:** One of `browser_process`, `renderer_processess`,
    `gpu_process`, or `all_processess`.
*   **source:** One of `reported_by_chrome` or `reported_by_os`
*   **component:** May be a Chrome component, e.g. `skia` or `sqlite`;
    details about a specific component, e.g. `v8:heap`; or a class of memory
    as seen by the OS, e.g. `system_memory:native_heap` or `gpu_memory`. If
    reported by chrome, the metrics are gathered by `MemoryDumpProvider`s,
    probes placed in the specific components' codebase. For example, in
    "memory:chrome:all_processes:reported_by_chrome:net:effective_size_avg,"
    the component is "net" which is Chrome's network stack and
    "reported_by_chrome" means that this metric is gathered via probes in
    the network stack.
*   **kind:** The kind of memory being reported. For metrics reported by
    Chrome this usually is `effective_size` (others are `locked_size`
    and `allocated_objects_size`); for metrics by the OS this usually is
    `proportional_resident_size` (others are `peak_resident_size` and
    `private_dirty_size`).

[memory-infra]: /docs/memory-infra/README.md
