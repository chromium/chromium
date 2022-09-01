# Using upstream wptrunner in Chromium (experimental)

This page documents the *experimental* support for using the upstream
[`wptrunner`](https://github.com/web-platform-tests/wpt/tree/master/tools/wptrunner/)
tooling for running WPT tests in Chromium (vs the [current
approach](web_platform_tests.md#Running-tests) that uses `run_web_tests.py`).

It is written as a user guide. For technical details on the project, see the
[design doc](https://docs.google.com/document/d/1Pq5fxR1t2JzOVPynqeRpRS4bM4QO_Z1um0Q_RiR5ETA/edit).

[TOC]

## Differences versus `run_web_tests.py`

The main differences between `run_web_tests.py` and `wptrunner` are that:

1. `wptrunner` runs the full `chrome` binary, rather than the stripped-down
   `content_shell`, and
1. `wptrunner` communicates with the binary via WebDriver (`chromedriver`),
   instead of talking directly to the browser binary.

These differences mean that any feature that works on upstream WPT today (e.g.
print-reftests) should work in `wptrunner`, but conversely features available to
`run_web_tests.py` (e.g. the `internals` API) are not yet available to
`wptrunner`.

## Running tests locally

*** note
**NOTE**: Running locally is an area of active development, so the following
instructions may be out of date.
***

The runner script is
[`third_party/blink/tools/run_wpt_tests.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/run_wpt_tests.py).
Before running the script, you must have built the necessary ninja targets:

```
autoninja -C out/Release wpt_tests_isolate
```

To run the script, enter chromium/src directory and run the command below:

```
./third_party/blink/tools/run_wpt_tests.py [test list]
```

The list of tests should be given relative to `external/wpt/`, e.g.
`webauthn/createcredential-timeout.https.html`. Directories are also accepted.
Omitting the test list will run all WPT tests.

Results from the run are placed in your build folder, in a folder called
`layout-test-results` (e.g. `../../out/Release/layout-test-results/`). Logs from
the browser should be shown by the runner as it executes each test.

Useful flags:

* `-t/--target`: select which `src/out/` sub-directory to use, e.g. `-t Debug`.
  Defaults to `Release`.
* `--help`: show the help text

## The MVP bots

As of Q4 2020, an MVP of wptrunner in Chromium is being run with two customer
teams: Web Payments and Web Identity. For these teams, two **Linux-only** bots
have been brought up:

* [linux-wpt-identity-fyi-rel](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-identity-fyi-rel),
  which runs tests under `external/wpt/webauthn/`.
* [linux-wpt-input-fyi-rel](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-input-fyi-rel),
  which runs tests under `external/wpt/{input-events, pointerevents, uievents}`,
  as well as `external/wpt/infrastructure/testdriver/actions/`

These bots run on the waterfall, but can also be run on CLs by clicking the
`Choose Tryjobs` button in Gerrit followed by searching for the bot name in the
modal dialog that appears. One can also include the tag `Cq-Include-Trybots:
luci.chromium.try:linux-wpt-identity-fyi-rel` (or input) in the description
for the CL, which will make the bot mandatory for that CL.

Results for the bots use the existing layout test results viewer
([example](https://test-results.appspot.com/data/layout_results/linux-wpt-identity-fyi-rel/201/wpt_tests_suite/layout-test-results/results.html)).

## Expectations and baseline files

[Similar to `run_web_tests.py`](web_test_expectations.md), `wptrunner` offers
the ability to add an expected status for a test or provide a baseline file that
codifies what the output result should be.

By default `wptrunner` will inherit expected statuses from `TestExpecations`.
This can currently be overridden by adding an entry to the
[`WPTOverrideExpectations`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/WPTOverrideExpectations)
file when `wptrunner` has a different result than `run_web_tests.py`.
`WPTOverrideExpectations` is however [deprecated](https://crbug.com/1035911),
and the preferred method for specifying expected results for `wptrunner` is to
use baseline files (which will also override a TestExpectation entry for the
test).

Baseline files for `wptrunner` use a different filename and format than
the `-expected.txt` files used by `run_web_tests.py`. The ini-like baseline format is
[documented here](https://web-platform-tests.org/tools/wptrunner/docs/expectation.html),
and baseline files should be placed alongside the test with an `.ini` suffix:

```
external/wpt/folder/my-test.html
external/wpt/folder/my-test-expected.txt <-- run_web_tests.py baseline
external/wpt/folder/my-test.html.ini <-- wptrunner baseline
```

We currently do not support the full ini-like format that upstream WPT does;
most notably we have chosen not to support dynamic conditionals (such as
platform checks). Most `.ini` baseline files in Chromium should have the form:

```
[my-test.html]
  expected: OK

  [First subtest name]
    expected: FAIL
    message: The failure message, e.g. assert_false didn't work

  [Second subtest name]
    expected: PASS
```

*** note
**TODO**: Explain how .any.js and variants work in this world.
***

## Known issues

* There is no debugging support in `run_wpt_tests.py` today. In the future, we
  intend to allow pausing the browser after each test, and (long-term) intend to
  support hooking up gdb to test runs.
* There is not yet support for non-Linux platforms. We would love for you to try
  it on other operating systems and file bugs against us if it doesn't work!

Please [file bugs and feature requests](https://crbug.com/new) against
`Blink>Infra>Ecosystem`, tagging the title with `[wptrunner]`.
