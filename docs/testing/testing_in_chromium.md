Testing is an essential component of software development in Chromium,
it ensures Chrome is behaving as we expect, and is critical to find bugs and
regressions at early stage.

This document covers the high level overview of testing in Chromium,
including what type of tests we have, what's the purpose for each test type,
what tests are needed for new features etc.

## Test Types

There are several different types of tests in Chromium to serve different purposes,
some types of test are running on multiple platforms, others are specific
for one platform.

*   **[gtest]** is Google's C++ test framework,
    which helps you write better C++ tests in Chromium.
    gtest is test framework for unit tests in Chromium and browser tests are built on top of it.
*   **[Junit]** is a unit testing framework
    for the Java programming language, and it is used to write
    unit tests on Android for Java code in Chromium.
*   **Browser Tests** is built on top of gtest, and it is used to write integration tests
    and e2e tests in Chromium.
    <!-- TODO(leilei) Add link to browser tests --->
*   **[Web Tests] (formerly known as "Layout Tests" or "LayoutTests")**
    is used by Blink to test many components, including but not
    limited to layout and rendering. In general, web tests involve loading pages
    in a test renderer (`content_shell`) and comparing the rendered output or
    JavaScript output against an expected output file.
    Web Tests are required to launch new W3C API support in Chromium.
*   **[Instrumentation Tests]** is a test framework specific for Android platform,
    it is used to write integration tests or e2e tests for Chromium on Android.
*   **[EarlGrey]** is the integration testing framework used by Chromium for iOS.
*   **[Telemetry]** is the performance testing framework used by Chromium.
    It allows you to perform arbitrary actions on a set of web pages and
    report metrics about it.
*   **[Fuzzer Tests]** is used to uncover potential security & stability problems in Chromium.
*   **[Tast]** is a test framework for system integration tests on Chrome OS.


The following table shows which types of test works on which platforms.

|                             |  Linux  | Windows |   Mac   | Android |  iOS    |  CrOS   |
|:----------------------------|:--------|:--------|:--------|:--------|:--------|:--------|
| gtest(C++)                  | &#8730; | &#8730; | &#8730; | &#8730; | &#8730; | &#8730; |
| Junit(Java)                 |         |         |         | &#8730; |         |         |
| Browser Tests(C++)          | &#8730; | &#8730; | &#8730; | &#8730; |         |         |
| Web Tests(HTML, JS)         | &#8730; | &#8730; | &#8730; |         |         |         |
| Telemetry(Python)           | &#8730; | &#8730; | &#8730; | &#8730; |         | &#8730; |
| Instrumentation Tests(Java) |         |         |         | &#8730; |         |         |
| EarlGrey                    |         |         |         |         | &#8730; |         |
| Fuzzer Tests(C++)           | &#8730; | &#8730; | &#8730; | &#8730; |         | &#8730; |
| Tast(Golang)                |         |         |         |         |         | &#8730; |

*** note
**Browser Tests Note**

Only subset of browser tests are enabled on Android:
*   components_browsertests
*   content_browsertests

Other browser tests are not supported on Android yet. [crbug/611756]
tracks the effort to enable them on Android.
***

*** note
**Web Tests Note**

Web Tests were enabled on Android K before, but it is disabled on Android platform now,
see [this thread](https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/338WKwWPbPI/discussion) for more context.
***

## General Principles

*   All the tests in Chromium running on CQ and main waterfall should be hermetic and stable.
*   Add unit tests along with the code in same changelist instead of adding tests in future,
    it is most likely no one will add tests later.
*   Write enough unit tests to have good [code coverage](./code_coverage.md),
    since they are fast and stable.
*   Don't enable tests with external dependencies on CQ and main waterfall,
    e.g. tests against live sites.
    It is fine to check in those tests, but only run them on your own bots.

## What tests are needed for new features

* **Unit Tests** are needed no matter where the code is for your feature.
  It is the best practice to add the unit tests
  when you add new code or update existing code in the same changelist,
  check out [Code Coverage in Gerrit](/code_coverage_in_gerrit.md)
  for the instruction about how to see the code coverage in Gerrit.
* **Browser Tests** are recommended for integration tests and e2e tests.
  It will be great if you add browser tests to cover the major user
  cases for your feature, even with some mocking.
* **[Web Tests]** are required if you plan to launch new W3C APIs in Chrome.
* **[Instrumentation Tests]** are recommended for features on Android, you only
  need to write instrumentation features
  if your feature is supported on Android for integration tests or e2e tests.
* **EarlGrey Tests** are recommended for iOS only.
* **[Telemetry] benchmarking or stories** are needed if existing telemetry
  benchmarks or stories can't cover the performance for your feature,
  you need to either add new story, but reuse existing metrics or
  add new benchmarks for your feature. Talk to benchmarking team first
  before start to add Telemetry benchmarks or stories.
* **[Fuzzer Tests]** are recommended if your feature adds user facing APIs
  in Chromium, it is recommended to write fuzzer tests to detect the security issue.

Right now, code coverage is the only way we have to measure test coverage.
The following is the recommended thresholds for different code coverage levels:
* >level 1(improving): >0%
* >level 2(acceptable): 60%
* >level 3(commendable): 75%
* >level 4(exemplary): 90%

Go to [code coverage dashboard](https://analysis.chromium.org/p/chromium/coverage) to check the code coverage for your project.


## How to write new tests
*  [Simple gtests]
*  [Writing JUnit tests]
*  [Writing Browser Tests]
*  [Writing Instrumentation Tests]
*  [Writing EarlGrey Tests]
*  [Writing Telemetry Benchmarks/Stories]
*  [Writing Web Tests](./writing_web_tests.md)
*  [Write Fuzz Target]

>TODO: add the link to the instruction about how to enable new tests in CQ and main waterfall

## How to run tests

### Run tests locally
*  [Run gtest locally]
*  [Run browser tests locally]
*  [Run tests on Android](./android_test_instructions.md#Running-Tests)
   It includes the instructions to run gTests, JUnit tests and Instrumentation tests on Android.
*  [Run EarlGrey tests locally](../ios/testing.md#running-tests-from-xcode)
*  [Run Web Tests locally](./testing/web_tests.md#running-web-tests)
*  [Telemetry: Run benchmarks locally]
*  [Run fuzz target locally]

### Run tests remotely(on Swarming)
>TODO: add the link to the instruction about how to run tests on Swarming.

## How to debug tests
*  [Android Debugging Instructions]
*  [Chrome OS Debugging Tips]
*  [Debugging Web Tests]

## How to deal with flaky tests

Go to [Flaky portal] to find the report about the flaky tests in your projects.

If you can not fix the flaky tests in a short time, consider to disable it first,
then fix it later. [How do I disable a flaky test] is the instruction about how to disable a flaky test.

>TODO: add the link to the instruction about how to reproduce/debug/verify flaky tests.


[gtest]: https://github.com/google/googletest
[Simple gtests]: https://github.com/google/googletest/blob/master/googletest/docs/primer.md#simple-tests
[Junit]: https://developer.android.com/training/testing/junit-rules
[Instrumentation Tests]: https://chromium.googlesource.com/chromium/src/+/master/testing/android/docs/instrumentation.md
[EarlGrey]: https://github.com/google/EarlGrey
[Telemetry]: https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md
[Fuzzer Tests]: https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/README.md
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/README.md
[Web Tests]: ./web_tests.md
[crbug/611756]: https://bugs.chromium.org/p/chromium/issues/detail?id=611756
[Flaky portal]: https://analysis.chromium.org/p/chromium/flake-portal
[Write Fuzz Target]: https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/getting_started.md#write-fuzz-target
[Telemetry: Run benchmarks locally]: https://chromium.googlesource.com/catapult/+/HEAD/telemetry/docs/run_benchmarks_locally.md
[Run fuzz target locally]: https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/getting_started.md#build-and-run-fuzz-target-locally
[Android Debugging Instructions]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_debugging_instructions.md
[Chrome OS Debugging Tips]: ./chromeos_debugging_tips.md
[Debugging Web Tests]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md#Debugging-Web-Tests
[code coverage dashboard]: https://analysis.chromium.org/p/chromium/coverage
[How do I disable a flaky test]: https://www.chromium.org/developers/tree-sheriffs/sheriff-details-chromium#TOC-How-do-I-disable-a-flaky-test-
