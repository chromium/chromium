# Running Web Platform Tests with run_wpt_tests.py

`run_web_tests.py` runs web tests with content shell through [protocol mode]. See
[web_tests.md](./web_tests.md) for details. `run_wpt_tests.py` instead can run web
platform tests with Chrome, Chrome Android and WebView. This document explains how
to use `run_wpt_tests.py` in these scenarios.

[TOC]

## Difference and Similarity with run_web_tests.py

`run_wpt_tests.py` can run with different browsers. To specify which browser to
run tests with, you should use `--product` or `-p`.  Supported parameters are `chrome`,
`chrome_android` (or `clank`), and `android_webview` (or `webview`). The default
value is `chrome` if not specified.

The CLI is mostly kept the same between run_web_tests.py and run_wpt_tests.py. To
see a complete list of arguments supported in run_wpt_tests.py, run:

```bash
third_party/blink/tools/run_wpt_tests.py --help
```

## Running Web Platform Tests with Chrome

### Supported Platforms

* Linux

It is possible to run tests for Chrome on other platforms, but test expectations
and baselines are only actively maintained for Linux due to resource constraint.

### Initial Setup

Before you can run the web platform tests, you need to build the `chrome_wpt_tests`
target to get `chrome`, `chromedriver` and all of the other needed binaries.

```bash
autoninja -C out/Default chrome_wpt_tests
```

### Running the Tests

Once you have `chrome` and `chromedriver` built, running tests is very much similar
to how you run tests with `run_web_tests.py`. For example, to run tests in `external/wpt/html/dom`,
you should run:

```bash
third_party/blink/tools/run_wpt_tests.py --release -p chrome third_party/blink/web_tests/external/wpt/html/dom
```

Note: consider using `-v` to get browser logs. It can be provided multiple times to
increase verbosity.

### Test expectations and Baselines

The
[ChromeTestExpectations](../../third_party/blink/web_tests/ChromeTestExpectations) file contains the list of all known Chrome
specific test failures, and it inherits or overrides test expectations from the default [TestExpectations](../../third_party/blink/web_tests/ChromeTestExpectations) file.
A special tag `Chrome` is introduced to specify chrome specific failures. See the
[Web Test Expectations documentation](./web_test_expectations.md) for more
on this.

Chrome specific baselines reside at `third_party/blink/web_tests/platform/linux-chrome`, and
falls back to `third_party/blink/web_tests/platform/linux`. To update baselines for chrome,
you should trigger `linux-wpt-chromium-rel` and run [rebaseline tool](./web_test_expectations.md#How-to-rebaseline) after the results are ready.

### Running webdriver tests with Chrome

Webdriver tests are one type (wdspec) of web platform tests. Due to this you can run webdriver tests
the same way as other web platform tests, e.g.

```bash
third_party/blink/tools/run_wpt_tests.py --release -p chrome external/wpt/webdriver/tests/classic/find_element/find.py
```

The `webdriver_wpt_tests` step of `linux-blink-rel` runs wdspec tests and can provide results for rebaselining.

## Running Web Platform Tests with Chrome Android

To be updated.

## Running Web Platform Tests with WebView

To be updated.

## Debugging Support

### Headful Mode

Passing the `--no-headless` flag to `run_wpt_tests.py` will pause execution
after running each test headfully.
You can interact with the paused test page afterwards, including with DevTools:

![Testharness paused](images/web-tests/wptrunner-paused.jpg)

Closing the tab or window will unpause the testharness and run the next test.

### Text-Based Debuggers

To interactively debug WPTs, prefix the `run_wpt_tests.py` command with
[`debug_renderer`][debug renderer] to attach a debugger to a desired renderer.

For other use cases, see [these debugging tips].

[these debugging tips]: /docs/linux/debugging.md

## Known Issues

Please [file bugs and feature requests](https://crbug.com/new) against
[`Blink>Infra` with the `wptrunner`
label](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EInfra%20label%3Awptrunner&can=2).

[protocol mode]: /content/web_test/browser/test_info_extractor.h
[debug renderer]: /third_party/blink/tools/debug_renderer
