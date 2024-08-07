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

Note: Internal testing APIs, e.g. `window.internals` or `window.testRunner`, are not available in Chrome. [Internal web
platform tests](../../third_party/blink/web_tests/wpt_internal) using those APIs should be skipped through [NeverFixTests](../../third_party/blink/web_tests/NeverFixTests).

### Supported Platforms

* Linux

Test expectations and baselines are only actively maintained for Linux due to
resource constraints.
It's not yet possible to run tests for Chrome on non-Linux platforms; follow
https://crbug.com/1512219 for status.

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

To suppress failures, `run_wpt_tests.py` uses the [same `*-expected.txt` and
TestExpectations files](web_test_expectations.md) that `run_web_tests.py` uses.

### Running webdriver tests with Chrome

Webdriver tests are one type (wdspec) of web platform tests. Due to this you can run webdriver tests
the same way as other web platform tests, e.g.

```bash
third_party/blink/tools/run_wpt_tests.py --release -p chrome external/wpt/webdriver/tests/classic/find_element/find.py
```

The `webdriver_wpt_tests` step of `linux-blink-rel` runs wdspec tests and can provide results for rebaselining.

## Running Web Platform Tests with Chrome Android

See [here](./run_web_platform_tests_with_chrome_android.md) for Android specific instructions.

## Running Web Platform Tests with WebView

To be updated.

## Debugging Support

### Text-Based Debuggers

To interactively debug WPTs, prefix the `run_wpt_tests.py` command with
[`debug_renderer`][debug renderer] to attach a debugger to a desired renderer.

For other use cases, see [these debugging tips].

[these debugging tips]: /docs/linux/debugging.md

## Known Issues

* Chromium's infrastructure currently tests WPTs against `chrome --headless=old`
  (i.e., the `//headless` layer, not `//chrome`). This may cause results to
  differ from [wpt.fyi] or `content_shell --run-web-tests` incorrectly. Notably,
  `//headless` will not apply features listed in
  [`fieldtrial_testing_config.json`][1]. See https://crbug.com/1485918 for
  updates on switching testing to [`chrome --headless=new`][2] for more useful
  results.

Please [file bugs and feature requests](https://crbug.com/new) against
[`Blink>Infra` with the `wptrunner`
label](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EInfra%20label%3Awptrunner&can=2).

[protocol mode]: /content/web_test/browser/test_info_extractor.h
[debug renderer]: /third_party/blink/tools/debug_renderer
[wpt.fyi]: https://wpt.fyi/results/?label=experimental&label=master&aligned

[1]: /testing/variations/fieldtrial_testing_config.json
[2]: https://developer.chrome.com/docs/chromium/new-headless
