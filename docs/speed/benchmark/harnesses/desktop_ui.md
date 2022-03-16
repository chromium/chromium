# Desktop UI Benchmark

## Overview

Desktop UI Benchmark is used to measure and monitor Desktop UI performance based on the [Telemetry](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md) framework.
It captures important metrics such as

* Initial load time
* Subsequent load time
* Fetch data time
* Search user input time
* Frame time when scrolling

in different stories representing different scenarios such as
* tab_search:top10:2020 - Test CUJs with 10 open tabs, after all tabs are loaded
* tab_search:top50:2020 - Test CUJs with 50 open tabs, after all tabs are loaded
* tab_search:top10:loading:2020 - Test CUJs with 10 open tabs, before all tabs are loaded
* tab_search:top50:loading:2020 - Test CUJs with 50 open tabs, before all tabs are loaded
* tab_search:close_and_open:2020 - Test open, close and reopen Tab Search UI
* tab_search:scroll_up_and_down:2020 - Test srolling down, up and down 100 tabs, before all tabs are loaded


For more information please see this [doc](https://docs.google.com/document/d/1-1ijT7wt05hlBZmSKjX_DaTCzVqpxbfTM1y-j7kYHlc?usp=sharing).

## Run the benchmark on pinpoint

In most cases, you only need to run the benchmark on [pinpoint](https://pinpoint-dot-chromeperf.appspot.com/) before/after a CL that may have performance impact. If you don't have platform requirement linux-perf is recommended since they are the most stable trybots for this benchmark.


## Run the benchmark locally

In some cases, if trybots cannot meet your requirement or you need to debug on your own machine, use the following command to run the benchmark locally. You need an @google account to run some of the recorded tests, or append --use-live-sites to the following command.

```
tools/perf/run_benchmark run desktop_ui --browser-executable=out/Default/chrome --story-filter=tab_search:top10:2020 --pageset-repeat=3
```

## Add new metrics

There are 3 ways to add metrics to the benchmarking code

1. Add UMA metrics to your code and include them in the [story definition](../../../../tools/perf/page_sets/desktop_ui/tab_search_story.py). The listed UMA metrics will show up on the result page automatically.
2. Set C++ trace event names as metric names to your story definition. It will write to the trace file and parsed by custom metric on the other end. For example:

    In C++, add trace events to measure performance
    ```c++
    void Foo::DoWork() {
      TRACE_EVENT0("browser", "Foo:DoWork");
      ...
    }
    ```

    In the story definition, set the trace events you want to see as a metric on the result page.
    ```python
    from page_sets.desktop_ui.custom_metric_utils import SetMetricNames
    ...
    def RunPageInteractions(self, action_runner):
      SetMetricNames(action_runner, ['Foo:DoWork'])
      ...
    ```
    or
    ```python
    from page_sets.desktop_ui.custom_metric_utils import SetMetrics
    ...
    def RunPageInteractions(self, action_runner):
      SetMetrics(action_runner, {'name':'Foo:DoWork','unit':<optional unit name>, 'description':<optional description>})
      ...
    ```

   This method requires 'customMetric' added to your story definition and enable the trace category 'blink.user_timing'. If your story extends from [MultiTabStory](../../../../tools/perf/page_sets/desktop_ui/multitab_story.py) it's already been taken care of. Also make sure to enable the trace categories you want to add in your story definition.

3. Add Javascript performance.mark() with names end with ":metric_begin" and ":metric_end". Time between performance.mark('<YOUR_METRIC_NAME>:metric_begin') and performance.mark('<YOUR_METRIC_NAME>:benchmark') will show up as YOUR_METRIC_NAME on the result page. For example:
   ```javascript
   function calc() {
     performance.mark('calc_time:metric_begin');
     ...
     performance.mark('calc_time:metric_end');
   }
   ```
   You can also emit metric value directly using performance.mark('<YOUR_METRIC_NAME>:<YOUR_METRIC_VALUE>:metric_value'). In the case multiple values needs to be measured asynchronously it's better to do the following instead:
   ```javascript
   const startTime = performance.now();
   for (const url of urls) {
     fetch(url).then(() => {
       performance.mark(`fetch_time:${performance.now() - startTime}:metric_value`);
     });
   }
   ```
   This method requires 'customMetric' added to your story definition and enable the trace category 'blink.user_timing'. If your story extends from [MultiTabStory](../../../../tools/perf/page_sets/desktop_ui/multitab_story.py) it's already been taken care of.

## How to write user journey

Destkop UI benchmark leverages UIDevtools to drive the native UI and Devtools to drive WebUI. Follow [existing stories](../../../../tools/perf/page_sets/desktop_ui/desktop_ui_stories.py) and the [demo CL](https://chromium-review.googlesource.com/c/chromium/src/+/2646447) to add user journey for most features.

Currently global shortcuts and context menus are not supported on Mac due to missing implementation of UIDevtools. Make sure to exclude those interaction on Mac from your performance stories.

## Add new stories

Add new stories to [here](../../../../tools/perf/page_sets/desktop_ui/desktop_ui_stories.py).
Generally we want to put most of the user journeys in the main story so we only need to run 1 story to verify a CL in most cases. However, if the user journey may affect metrics of other user journeys, you should make it a separate story.

- If your new stories don't need external network requests, you are done now.
- If your new stories need external network requests and extend from MultiTabStory or other existing stories, add an item to [here](../../../../tools/perf/page_sets/data/desktop_ui.json) to reuse some of the existing recorded stories
- If your new stories need external network requests that can't be reused from existing stories, follow the next section to record your own stories.

Finally, run the new stories locally to make sure they work, then upload the CL and run a pinpoint job to make sure they work before starting code review.

## Record new stories

Record and upload new stories to cloud storage using
[these instructions](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/recording_benchmarks.md).

## Query benchmark metrics from chrome perf waterfall

Use the following link to query and compare realtime metrics data from any perf bots running on the performance waterfall.

https://chromeperf.appspot.com/report

## Setup auto-triage on perf waterfall

Follow the [auto-triage doc](http://go/chromeperf-auto-triage) to setup metrics you want to monitor for regression([Example CL](https://chrome-internal-review.googlesource.com/c/infra/infra_internal/+/3897062)).
If there's a regression detected on the waterfall an email will sent out automatically. You can then auto or manual bisect what commit regresses the performance.