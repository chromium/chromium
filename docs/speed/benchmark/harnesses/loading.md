# Loading benchmarks

[TOC]

## Overview

The Telemetry loading benchmark measure Chrome's loading performance under
different network and caching conditions.

There is currently one loading benchmarks:

- **`loading.cluster_telemetry`**: A cluster Telemetry benchmark that uses the
corpus of top 10 thousands URLs from Alexa. This benchmark is triggered
on-demand only.

## Running the tests remotely

If you're just trying to gauge whether your change has caused a loading
regression, you can run the loading benchmark through the Cluster Telemetry
service. You can specify a Gerrit patch for them and compare the results with
and without the patch applied.

### Using Cluster Telemetry Service
You can run `loading.cluster_telemetry` (top 10k pages) through
[Cluster Telemetry service](https://ct.skia.org/) (Cluster Telemetry is for
Googler only).

## Understanding the loading test cases

The loading test cases are divided into groups based on their network traffic
settings and cache conditions.

All available traffic settings can be found in [traffic_setting.py](https://chromium.googlesource.com/catapult/+/main/telemetry/telemetry/page/traffic_setting.py)

All available caching conditions can be found in [cache_temperature.py](https://chromium.googlesource.com/catapult/+/main/telemetry/telemetry/page/cache_temperature.py)

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
