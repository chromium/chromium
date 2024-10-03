# Running Web Platform Tests with run_wpt_tests.py

`run_web_tests.py` runs web tests with content shell through [protocol mode].
See [web_tests.md](web_tests.md) for details.
`run_wpt_tests.py` is a WebDriver-based alternative that can run [web platform
tests] with [Chrome], [headless shell], Chrome Android, and WebView.
This document explains how to use `run_wpt_tests.py` in these scenarios.

[web platform tests]: web_platform_tests.md
[Chrome]: /chrome
[headless shell]: /headless

[TOC]

## Running Web Platform Tests for Desktop Platforms

On Linux, macOS, and Windows, `run_wpt_tests.py` supports testing with [Chrome]
or [headless shell].
Chrome is closer to the binary Google ships to end users, but is generally
slower.
Headless shell is a lightweight alternative that suffices for testing features
implemented entirely in Blink.

### Running Tests Locally

First, you will need to build one of the following targets to get all needed
binaries:

```bash
autoninja -C out/Default chrome_wpt_tests     # For testing with `chrome`
autoninja -C out/Default headless_shell_wpt   # For testing with `headless_shell`
```

Once the build is done, running tests is very similar to how you would run
tests with `run_web_tests.py`.
For example, to run all tests under `external/wpt/html/dom`, run:

```bash
third_party/blink/tools/run_wpt_tests.py --target=Default --product=headless_shell external/wpt/html/dom
```

`--product` (or `-p`) selects which browser to test with.
Supported values are:

* `headless_shell` (default if `--product` is not specified)
* `chrome`
* `chrome_android` (aliased as `clank`; see
  [additional instructions](#Running-Web-Platform-Tests-on-Android))
* `android_webview` (aliased as `webview`; see
  [additional instructions](#Running-Web-Platform-Tests-on-Android))

Also, consider using `-v` to get browser logs.
It can be provided multiple times to increase verbosity.

`run_wpt_tests.py --help` shows a full description of `run_wpt_tests.py`'s CLI,
which resembles that of `run_web_tests.py`.

### Running Tests in CQ/CI

To satisfy different testing requirements, WPT coverage in CQ/CI is partitioned
between suites that target different `//content` embedders:

Suite Name | Browser Under Test | Harness | Tests Run
--- | --- | --- | ---
`blink_wpt_tests` | `content_shell --run-web-tests` | `run_web_tests.py` | Tests that depend on web test-specific features (e.g., [internal WPTs] that depend on [nonstandard `window.internals` or `window.testRunner` APIs][3]).
`chrome_wpt_tests` | `chrome --headless=new` | `run_wpt_tests.py` | Tests that depend on the `//chrome` layer. Can be slow, so prefer `headless_shell` testing if possible.
`headless_shell_wpt_tests` | `headless_shell` | `run_wpt_tests.py` | All other tests. Most WPTs should eventually run here.

To avoid redundant coverage, each WPT should run in exactly one suite listed
above.
The [`chrome.filter`][1] file lists tests that `chrome_wpt_tests` should run,
and that `headless_shell_wpt_tests` and `blink_wpt_tests` should skip.
[`headless_shell.filter`][2] works similarly.
Tests not listed in either file run in `blink_wpt_tests` by default.

*** note
Running tests in `blink_wpt_tests` is discouraged because `run_web_tests.py`
doesn't drive tests through standard WebDriver endpoints.
This can cause `blink_wpt_tests` results to diverge from the Chrome results
published to [wpt.fyi].
You can help unblock the eventual deprecation of `blink_wpt_tests` by adding
tests that you own to either filter file.
***

[internal WPTs]: /third_party/blink/web_tests/wpt_internal

### Test Expectations and Baselines

To suppress failures, `run_wpt_tests.py` uses the [same `*-expected.txt` and
TestExpectations files](web_test_expectations.md) that `run_web_tests.py` uses.

### Running webdriver tests with Chrome

[wdspec tests] are a subset of WPT that verifies conformance to the WebDriver
specification.
`run_wpt_tests.py` can run wdspec tests like any other WPT:

```bash
third_party/blink/tools/run_wpt_tests.py -t Default -p chrome \
  external/wpt/webdriver/tests/classic/find_element/find.py
```

On the bots, the `webdriver_wpt_tests` suite runs wdspec tests separately from
the other WPT types.
The `linux-blink-rel` builder can provide results for rebaselining.

[wdspec tests]: https://web-platform-tests.org/writing-tests/wdspec.html

## Running Web Platform Tests on Android

See [here](./run_web_platform_tests_on_android.md) for Android specific instructions.

## Debugging Support

### Text-Based Debuggers

To interactively debug WPTs, prefix the `run_wpt_tests.py` command with
[`debug_renderer`][debug renderer] to attach a debugger to a desired renderer.

For other use cases, see [these debugging tips].

[these debugging tips]: /docs/linux/debugging.md

## FAQ

* Do headless shell and Chrome support MojoJS bindings?
    * Yes.
      `run_wpt_tests.py` enables the `MojoJS` and `MojoJSTest` features and
      serves `//out/<target>/gen/` as `/gen/` in wptserve.
      However, in the public WPT suite, testdriver.js APIs backed by standard
      WebDriver endpoints should be preferred over polyfills backed by MojoJS,
      which are Chromium-specific.
      See https://github.com/web-platform-tests/rfcs/issues/172 for additional
      discussion.

## Known Issues

The [`wptrunner-migration`
hostlist](https://issues.chromium.org/hotlists/6224346) tracks test results
where headless shell and content shell differ.
For runner bugs and feature requests, please file [an issue against
`Blink>Infra`](https://issues.chromium.org/issues/new?component=1456928&template=1923166).

[protocol mode]: /content/web_test/browser/test_info_extractor.h
[debug renderer]: /third_party/blink/tools/debug_renderer
[wpt.fyi]: https://wpt.fyi/results/?label=experimental&label=master&aligned

[1]: /third_party/blink/web_tests/TestLists/chrome.filter
[2]: /third_party/blink/web_tests/TestLists/headless_shell.filter
[3]: writing_web_tests.md#Relying-on-Blink_Specific-Testing-APIs
