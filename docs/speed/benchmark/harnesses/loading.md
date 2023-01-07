# Loading benchmarks

[TOC]

## Overview

The Telemetry loading benchmarks measure Chrome's loading performance under
different network and caching conditions.

There are currently three loading benchmarks:

- **`loading.desktop`**: A desktop-only benchmark in which each test case
  measures performance of loading a real world website (e.g: facebook, cnn,
  alibaba..).
- **`loading.mobile`**: A mobile-only benchmark that parallels `loading.desktop`
- **`loading.cluster_telemetry`**: A cluster Telemetry benchmark that uses the
corpus of top 10 thousands URLs from Alexa. Unlike the other two loading
benchmarks which are run continuously on the perf waterfall, this benchmark is
triggered on-demand only.

## Running the tests remotely

If you're just trying to gauge whether your change has caused a loading
regression, you can run the loading benchmarks through perf try job or through
Cluster Telemetry service. You can specify a Gerrit patch for them and compare
the results with and without the patch applied.

### Using Perf Try Bots
[Perf Try Bots](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/perf_trybots.md)
lets you run only single story of `loading.desktop` or `loading.mobile`, because
an entire loading benchmark is too large to run as a perf try job. You should
only set the `Story` field, and leave the `Story Tags` field blank. Invalid
combinations of `Story` and `Story Tags` will result in "zero Histograms" result
page.

See
[Understanding the loading test cases](#understanding-the-loading-test-cases)
for story name conventions.

### Using Cluster Telemetry Service
You can run `loading.cluster_telemetry` (top 10k pages) through
[Cluster Telemetry service](https://ct.skia.org/) (Cluster Telemetry is for
Googler only).

## Running the tests locally

For more in-depth analysis and shorter cycle times, it can be helpful to run the tests locally.

First, [prepare your test device for
Telemetry](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/telemetry_device_setup.md).

Once you've done this, you can start the Telemetry benchmark with:

```
./tools/perf/run_benchmark <benchmark_name> --browser=<browser>
```

where `benchmark_name` can be `loading.desktop` or `loading.mobile`.

## Understanding the loading test cases

The loading test cases are divided into groups based on their network traffic
settings and cache conditions.

All available traffic settings can be found in [traffic_setting.py](https://chromium.googlesource.com/catapult/+/main/telemetry/telemetry/page/traffic_setting.py)

All available caching conditions can be found in [cache_temperature.py](https://chromium.googlesource.com/catapult/+/main/telemetry/telemetry/page/cache_temperature.py)

Test cases of `loading.desktop` and `loading.mobile` are named with their
corresponding settings. For example, `DevOpera_cold_3g` test case loads
`https://dev.opera.com/` with cold cache and 3G network setting.

In additions, the pages are also tagged with labels describing their content.
e.g: 'global', 'pwa',...

To run only pages of one tags, add `--story-tag-filter=<tag name>` flag to the
run benchmark command.

## Understanding the loading metrics
The benchmark output several different loading metrics. The keys one are:
 * [Time To First Contentful Paint](https://docs.google.com/document/d/1kKGZO3qlBBVOSZTf-T8BOMETzk3bY15SC-jsMJWv4IE/edit#heading=h.27igk2kctj7o)
 * [Time To First Meaningful Paint](https://docs.google.com/document/d/1BR94tJdZLsin5poeet0XoTW60M0SjvOJQttKT-JK8HI/edit)
 * [Time to First CPU
   Idle](https://docs.google.com/document/d/12UHgAW2r7nWo3R6FBerpYuz9EVOdG1OpPm8YmY4yD0c/edit#)

Besides those key metrics, there are also breakdown metrics that are meant to
to make debugging regressions simpler. These metrics are updated often, for most
up to date information, you can email speed-metrics-dev@chromium.org
or chrome-speed-metrics@google.com (Googlers only).

## Adding new loading test cases
New test cases can be added by modifying
[loading_desktop.py](https://chromium.googlesource.com/chromium/src/+/main/tools/perf/page_sets/loading_desktop.py)
or [loading_mobile.py](https://chromium.googlesource.com/chromium/src/+/main/tools/perf/page_sets/loading_mobile.py) page sets.

For example, to add a new case of loading
`https://en.wikipedia.org/wiki/Cats_and_the_Internet` on 2G and 3G networks with
warm cache to `news` group to `loading.desktop` benchmark, you would write:

```
self.AddStories(
  tags=['news'],
  urls=[('https://en.wikipedia.org/wiki/Cats_and_the_Internet', 'wiki_cats')],
  cache_temperatures=[cache_temperature_module.WARM],
  traffic_settings=[traffic_setting_module.2G, traffic_setting_module.3G])
```

After adding the new page, record and upload it by following
[these instructions](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/recording_benchmarks.md).
