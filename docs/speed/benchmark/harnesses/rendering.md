# Rendering Benchmarks

This document provides an overview of the benchmarks used to monitor Chrome’s graphics performance. It includes information on what benchmarks are available, how to run them, how to interpret their results, and how to add more tests to the benchmarks.

[TOC]

## Glossary

-   **Page** (or story): A recording of a website, which is associated with a set of actions (ex. scrolling)
-   **Page Set** (or story set): A collection of different pages, organized by some shared characteristic (ex. top real world mobile sites)
-   **Metric**: A process that describes how to collect meaningful data from a Chrome trace and calculate results (ex. frame time)
-   **Benchmark**: A combination of a page set and multiple metrics
-   **Telemetry**: The [framework](https://github.com/catapult-project/catapult/blob/master/telemetry/README.md) used for Chrome performance testing, which allows benchmarks to be run and metrics to be collected

## Overview

The Telemetry rendering benchmarks measure Chrome’s rendering performance in different scenarios.

There are currently two rendering benchmarks:

-   `rendering.desktop`: A desktop-only benchmark that measures performance on both real world websites and special cases (ex. pages that are difficult to zoom)
-   `rendering.mobile`: A mobile-only equivalent of rendering.desktop

Note: Some pages are used for rendering.desktop but not rendering.mobile, and vice versa. This is because some pages are only meant to measure behavior on one platform, for instance dragging on desktop. This is indicated with the `SUPPORTED_PLATFORMS` attribute in the page class.

These benchmarks are run on the [Chromium Perf Waterfall](https://ci.chromium.org/p/chromium/g/chromium.perf/console), with results reported on the [Chrome Performance Dashboard](https://chromeperf.appspot.com/report).

## What are the  rendering metrics

Rendering metrics are [written in Javascript](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/rendering). The list of all metrics and their meanings should be documented in the files they are defined in.

-   [cpu\_utilization.html](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/rendering/cpu_utilization.html): `cpu_time_per_frame` and `tasks_per_frame`
-   [frame\_time.html](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/rendering/frame_time.html): `frame_times`, `percentage_smooth`, `frame_lengths`, `avg_surface_fps`, `jank_count`, and `ui_frame_times`
-   [pixels.html](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/rendering/pixels.html): `mean_pixels_approximated` and `mean_pixels_checkerboarded`
-   [queueing\_duration.html](https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/metrics/rendering/queueing_duration.html): `queueing_durations`

## How to run rendering benchmarks on local devices

First, set up your device by following the instructions [here](https://chromium.googlesource.com/chromium/src/+/master/docs/speed/benchmark/telemetry_device_setup.md). You can then run telemetry benchmarks locally using:

`./tools/perf/run_benchmark <benchmark_name> --browser=<browser>`

For `benchmark_name`, use either `rendering.desktop` or `rendering.mobile`

As the pages in the rendering page sets were merged from a variety of previous page sets, they have corresponding tags. To run the benchmark only for pages of a certain tag, add this flag:

`--story-tag-filter=<tag name>`

For example, if the old benchmark was `smoothness.tough_scrolling_cases`, you would now use `--story-tag-filter=tough_scrolling` for the rendering benchmarks. A list of all rendering [tags](https://cs.chromium.org/chromium/src/tools/perf/page_sets/rendering/story_tags.py?dr&g=0) can be found here. You can also find out which tags are used by a page by looking at the `TAGS` attribute of the class. Additionally, these same tags can be used to filter the metrics results in the generated results.html file.

Other useful options for the command are:

-   `--pageset-repeat [n]`: override the default number of repetitions
-   `--reset-results`: clear results from any previous benchmark runs in the results.html file.
-   `--results-label [label]`: give meaningful names to your benchmark runs, to make it easier to compare them

## How to run rendering benchmarks on try bots

For more consistent results and to identify whether your change has resulted in a rendering regression, you can run the rendering benchmarks using a [perf try job](https://chromium.googlesource.com/chromium/src/+/master/docs/speed/perf_trybots.md). In order to do this, you need to first upload a CL, which allows results to be generated with and without your patch.

## How to handle regressions

If your changes have resulted in a regression in a metric that is monitored by [perf alerts](https://chromeperf.appspot.com/alerts?sortby=end_revision&sortdirection=down), you will be assigned to a bug. This will contain information about the specific metric and how much it was regressed, as well as a Pinpoint link that will help you investigate further. For instance, you will be able to obtain traces from the try bot runs. This [link](https://chromium.googlesource.com/chromium/src/+/master/docs/speed/addressing_performance_regressions.md) contains detailed steps on how to deal with regressions. Rendering metrics use trace events logged under the benchmark and toplevel trace categories.

If you already have a trace and want to debug the metric computation part, you can just run the metric:
`tracing/bin/run_metric <path-to-trace-file> renderingMetric`

## How to add more pages

New rendering pages should be added to the [./tools/perf/page_sets/rendering](https://cs.chromium.org/chromium/src/tools/perf/page_sets/rendering/?dr&g=0) folder:

Pages inherit from the [RenderingStory](https://cs.chromium.org/chromium/src/tools/perf/page_sets/rendering/rendering_story.py?dr&g=0) class. If adding a group of new pages, create an abstract class with the following attributes:

-   `ABSTRACT_STORY = True`
-   `TAGS`: a list of tags, which can be added to [story_tags.py](https://cs.chromium.org/chromium/src/tools/perf/page_sets/rendering/story_tags.py?dr&g=0) if necessary
-   `SUPPORTED_PLATFORMS` (optional): if the page should only be mobile or desktop

Children classes should specify these attributes:
-   `BASE_NAME`: name of the page
	- Use the “new_page_name” format
	- If the page is a real-world website and should be periodically refreshed, add “_year” to the end of the page name and update the value when a new recording is uploaded
	- Ex. google_web_search_2018
-   `URL`: url of the page

All pages in the rendering benchmark need to use [RenderingSharedState](https://cs.chromium.org/chromium/src/tools/perf/page_sets/rendering/rendering_shared_state.py?dr&g=0) as the shared_page_state_class, since this has to be consistent across pages in a page set. Individual pages can also specify `extra_browser_args`, in order to set specific flags.

After adding the page, record it and upload it to cloud storage using:

`./tools/perf/record_wpr rendering_desktop --browser=system --story-tag-filter=<tag name> --upload`

This will modify the [data/rendering_desktop.json](https://cs.chromium.org/chromium/src/tools/perf/page_sets/data/rendering_desktop.json?type=cs&q=rendering_deskt&g=0&l=1) or [data/rendering_mobile.json](https://cs.chromium.org/chromium/src/tools/perf/page_sets/data/rendering_mobile.json?type=cs&g=0) files and generate .sha1 files, which should be included in the CL.

### Merging existing pages

If more pages need to be merged into the rendering page sets, please see [this guide](https://docs.google.com/document/d/19vUZCnJ0_5pfcwotl0ABTFGFIBc_CckNIyfE7Cs7I3o/edit#bookmark=id.w3jf2ip73aat) on how to do so.
