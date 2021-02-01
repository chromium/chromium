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
* tab_search:top100:2020 - Test CUJs with 100 open tabs, after all tabs are loaded
* tab_search:top10:loading:2020 - Test CUJs with 10 open tabs, before all tabs are loaded
* tab_search:top50:loading:2020 - Test CUJs with 50 open tabs, before all tabs are loaded
* tab_search:top100:loading:2020 - Test CUJs with 100 open tabs, before all tabs are loaded
* tab_search:close_and_open:2020 - Test open, close and reopen Tab Search UI
* tab_search:scroll_up_and_down:2020 - Test srolling down, up and down 100 tabs, before all tabs are loaded


For more information please see this [doc](https://docs.google.com/document/d/1-1ijT7wt05hlBZmSKjX_DaTCzVqpxbfTM1y-j7kYHlc).

## Run the benchmark on pinpoint

In most cases, you only need to run the benchmark on [pinpoint](https://pinpoint-dot-chromeperf.appspot.com/) before/after a CL that may have performance impact. If you don't have platform requirement linux-perf is recommended since they are the most stable trybots for this benchmark.


## Run the benchmark locally

In some cases, if trybots cannot meet your requirement or you need to debug on your own machine, use the following command to run the benchmark locally. You need an @google account to be able to do that.

```
tools/perf/run_benchmark run desktop_ui --browser-executable=out/Default/chrome --story-filter=tab_search:top10:2020 --pageset-repeat=3
```


## Add new metrics

There are 3 ways to add metrics to the benchmarking code

1. Add UMA metrics to your code and include them in the [story definition](../../../../tools/perf/page_sets/desktop_ui/tab_search_story.py). The listed UMA metrics will show up on the result page automatically.
2. Add C++ trace with name starts with "custom_metric:". For example:
   ```c++
   void Foo::DoWork() {
     TRACE_EVENT0("browser", "custom_metric:Foo:DoWork");
     ...
   }
   ```
   This method requires 'customMetric' added to your story definition. If your story extends from [MultiTabStory](../../../../tools/perf/page_sets/desktop_ui/multitab_story.py) it's already been taken care of. Also make sure to enable the trace categories you want to add in your story definition.

3. Add Javascript performance.mark() with names end with ":benchmark_begin" and ":benchmark_end". Time between performance.mark('<YOUR_METRIC_NAME>:benchmark_begin') and performance.mark('<YOUR_METRIC_NAME>:benchmark') will show up as YOUR_METRIC_NAME on the result page. For example:
   ```javascript
   function calc() {
     performance.mark('calc_time:benchmark_begin');
     ...
     performance.mark('calc_time:benchmark_end');
   }
   ```
   You can also emit metric value directly using performance.mark('<YOUR_METRIC_NAME>:<YOUR_METRIC_VALUE>:benchmark_value'). In the case multiple values needs to be measured asynchronously it's better to do the following instead:
   ```javascript
   const startTime = performance.now();
   for (const url of urls) {
     fetch(url).then(() => {
       performance.mark(`fetch_time:${performance.now() - startTime}:benchmark_value`);
     });
   }
   ```
   This method requires 'customMetric' added to your story definition and enable the trace category 'blink.user_timing'. If your story extends from [MultiTabStory](../../../../tools/perf/page_sets/desktop_ui/multitab_story.py) it's already been taken care of.


## Add new stories

Add new stories to [here](../../../../tools/perf/page_sets/desktop_ui/desktop_ui_stories.py).
Generally we want to put most of the user journeys in the main story so we only need to run 1 story to verify a CL in most cases. However, if the user journey may affect metrics of other user journeys, you should make it a separate story.

- If your new stories don't need external network requests, you are done now.
- If your new stories need external network requests and extend from MultiTabStory or other existing stories, add an item to [here](../../../../tools/perf/page_sets/data/desktop_ui.json) to reuse some of the existing recorded stories
- If your new stories need external network requests that can't be reused from existing stories, follow the next section to record your own stories.

Finally, run the new stories locally to make sure they work, then upload the CL and run a pinpoint job to make sure they work before starting code review.

## Record new stories

Use the following command to record a story
```
tools/perf/record_wpr --browser-executable=out/Default/chrome desktop_ui --story-filter=<YOUR_STORY_NAME>
```
and the following command to upload to the cloud.
```
upload_to_google_storage.py --bucket chrome-partner-telemetry tools/perf/page_sets/data/desktop_ui_<YOUR_RECORDED_HASH>.wprgo
```