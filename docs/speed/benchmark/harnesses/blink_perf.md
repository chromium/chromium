# Blink Performance Tests

[TOC]

## Overview

Blink perf tests are used for micro benchmarking the surface of Blink that
is exposed to the Web. They are the counterpart of [web_tests/](../../../testing/web_tests.md)
but for performance coverage.

## Writing Tests
Each test entry point is a HTML file written using
[runner.js](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/resources/runner.js)
testing framework. The test file is placed inside a sub folder of
[blink/perf_tests/](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/)
and is started by importing `runner.js` script into the document:
```
  <script src="../resources/runner.js"></script>

```

### Synchronous Perf Tests
In a nutshell, to measure speed of synchronous code encapsulated in a test run
method `F`, synchronous perf tests exercises this loop:

```
   FOR i = 1 to NUMBER_OF_REPEAT
      Start timer
      F()
      Stop timer
```

Depending on how fast `F` runs, one can choose between
`PerfTestRunner.measureTime` or `PerfTestRunner.measureRunsPerSecond`
(very fast). In either case, you create a test object & run by invoking the
measure method as follow:

```
PerfTestRunner.measureTime({  // the "test" object
   description: '...',
   setup: function () { ... },  // test setup logic, called once before each run
   run: function () { ... },  // contains the code to benchmark
   iterationCount: 5   // repeat the test 5 times
});
```

In the case of `PerfTestRunner.measureRunsPerSecond`, each run invokes
`test.run` multiple times.

**Tracing support**

When the test is run through Telemetry, you can also collect timing of trace
events that happen during each run by specifying `tracingCategories` &
`traceEventsToMeasure` in the test object. For example:

```
PerfTestRunner.measureTime({
   ...
   run: foo,
   iterationCount: 3,
   tracingCategories: 'blink',
   traceEventsToMeasure: ['A', 'B'],
});
```
To illustrate what the framework computes, imaging the test timeline as
follow:

```
Test run times (time duration of each slice is right under it):
-----------[       foo        ]-----[   foo       ]-----[    foo        ]------
                    u1                   u2                  u3
---------------[    A   ]------------------------------------------------------
                     v0
-----------------[ A  ]--[ A ]---------[ B  ]--[A]----------[ B  ]--[C]--------
                   v1      v2            v3     v4            v5     v6
```

Besides outputting timeseries `[u1, u2, u3]`, telemetry perf test runner will
also compute the total CPU times for trace events  'A' & 'B' per `foo()` run:

*   CPU times of trace events A: `[v0 + v2, v4, 0.0]`
*   CPU times of trace events B: `[0.0, v3, v5]`

Example tracing synchronous tests:

*   [append-child-measure-time.html](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/test_data/append-child-measure-time.html)

*   [simple-html-measure-page-load-time.html](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/test_ata/simple-html-measure-page-load-time.html)


### Asynchronous Perf Tests
In asynchronous perf test, you define your test scheduler and do your own
measurement. For example:

```
var isDone = false;
var startTime;

function runTest() {
    if (startTime) {
        PerfTestRunner.measureValueAsync(PerfTestRunner.now() - startTime);
        PerfTestRunner.addRunTestEndMarker(); // For tracing metrics
    }
    if (!isDone) {
        PerfTestRunner.addRunTestStartMarker();
        startTime = PerfTestRunner.now();  // For tracing metrics
        // runTest will be invoked after the async operation finish
        runAsyncOperation(runTest);
    }
}

PerfTestRunner.startMeasureValuesAsync({
    unit: 'ms',
    done: function () {
        isDone = true;
    },
    run: function() {
        runTest();
    },
    iterationCount: 6,
});
```

In the example above, the call
`PerfTestRunner.measureValueAsync(value)` send the metric of a single run to
the test runner and also let the runner know that it has finished a single run.
Once the number of run reaches `iterationCount` (6 in the example above), the
`done` callback is invoked, setting the your test state to finished.

**Tracing support**

Like synchronous perf tests, tracing metrics are only available when you run
your tests with Telemetry.

Unlike synchronous perf tests which the test runner framework handles test
scheduling and tracing coverage for you, for most asynchronous tests, you need
to manually mark when the async test begins
(`PerfTestRunner.addRunTestStartMarker`) and ends
(`PerfTestRunner.addRunTestEndMarker`). Once those are marked, specifying
`tracingCategories` and `traceEventsToMeasure` will output CPU time metrics
of trace events that happen during test runs in the fashion similar to the
example of synchronous tracing test above.

Example of tracing asynchronous tests:

[color-changes-measure-frame-time.html](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/test_data/color-changes-measure-frame-time.html)

[simple-blob-measure-async.html](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/test_data/simple-blob-measure-async.html)

## Canvas Tests

The sub-framework [canvas_runner.js](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/canvas/resources/canvas_runner.js) is used for
tests in the `canvas` directory. This can measure rasterization and GPU time
using requestAnimationFrame (RAF) and contains a callback framework for video.

Normal tests using `runTest()` work similarly to the asynchronous test above,
but crucially wait for RAF after completing a single trial of
`MEASURE_DRAW_TIMES` runs.

RAF tests are triggered by appending the query string `raf` (case insensitive)
to the test's url. These tests wait for RAF to return before making a
measurement. This way rasterization and GPU time are included in the
measurement.

For example:

The test [gpu-bound-shader.html](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/perf_tests/canvas/gpu-bound-shader.html) is just measuring
CPU, and thus looks extremely fast as the test is just one slow shader.

The url `gpu-bound-shader.html?raf` will measure rasterization and GPU time as
well, thus giving a more realistic measurement of performance.

## Running Tests

**Running tests directly in browser**

Most of Blink Performance tests should be runnable by just open the test file
directly in the browser. However, features like tracing metrics & HTML results
viewer won't be supported.

**Running tests with Telemetry**

Assuming your current directory is `chromium/src/`, you can run tests with:

`./tools/perf/run_benchmark run blink_perf [--test-path=<path to your tests>]`

For information about all supported options, run:

`./tools/perf/run_benchmark run blink_perf --help`
