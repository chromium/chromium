# Using wptrunner in Chromium (experimental)

[`wptrunner`](https://github.com/web-platform-tests/wpt/tree/master/tools/wptrunner)
is the harness shipped with the WPT project for running the test suite. This
user guide documents *experimental* support in Chromium for `wptrunner`, which
will replace [`run_web_tests.py`](web_platform_tests.md#Running-tests) for
running WPTs in CQ/CI.

For general information on web platform tests, see
[web-platform-tests.org](https://web-platform-tests.org/test-suite-design.html).

For technical details on the migration to `wptrunner` in Chromium, see the
[project plan](https://docs.google.com/document/d/1VMt0CB8LO_oXHh7OIKsG-61j4nusxPnTuw1v6JqsixY/edit?usp=sharing&resourcekey=0-XbRB7-vjKAg5-s2hWhOPkA).

*** note
**Warning**: The project is under active development, so expect some rough
edges. This document may be stale.
***

[TOC]

## Differences from `run_web_tests.py`

The main differences between `run_web_tests.py` and `wptrunner` are that:

1. `wptrunner` can run both the full `chrome` binary and the stripped-down
   `content_shell`. `run_web_tests.py` can only run `content_shell`.
1. `wptrunner` can communicate with the binary via WebDriver (`chromedriver`),
   instead of talking directly to the browser binary.

These differences mean that any feature that works on upstream WPT today (e.g.
print-reftests) should work in `wptrunner`, but conversely, features available to
`run_web_tests.py` (e.g. the `internals` API) are not yet available to
`wptrunner`.

## Running tests locally

The `wptrunner` wrapper script is
[`//third_party/blink/tools/run_wpt_tests.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/run_wpt_tests.py).
First, build the necessary ninja target:

``` sh
autoninja -C out/Release wpt_tests_isolate_content_shell
```

To run the script, run the command below from `//third_party/blink/tools`:

``` sh
./run_wpt_tests.py [test list]
```

Test paths should be given relative to `blink/web_tests/` (*e.g.*,
[`wpt_internal/badging/badge-success.https.html`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/wpt_internal/badging/badge-success.https.html)).
For convenience, the `external/wpt/` prefix can be omitted for the [external test
suite](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt/)
(*e.g.*,
[`webauthn/createcredential-timeout.https.html`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt/webauthn/createcredential-excludecredentials.https.html)).

`run_wpt_tests.py` also accepts directories, which will run all tests under
those directories.
Omitting the test list will run all WPT tests (both internal and external).
Results from the run are placed under `//out/<target>/layout-test-results/`.

Useful flags:

* `-t/--target`: Select which `//out/` subdirectory to use, e.g. `-t Debug`.
  Defaults to `Release`.
* `-p/--product`: Select which browser (or browser component) to test. Defaults
  to `content_shell`, but choices also include [`chrome`, `chrome_android`, and
  `android_webview`](https://source.chromium.org/search?q=run_wpt_tests.py%20lang:gn).
* `-v`: Increase verbosity (may provide multiple times).
* `--help`: Show the help text.

## Experimental Builders

As of Q4 2022, `wptrunner` runs on a handful of experimental FYI CI builders
(mostly Linux):

* [`linux-wpt-content-shell-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-content-shell-fyi-rel),
  which runs content shell against `external/wpt/` and `wpt_internal/`
* [`win10-wpt-content-shell-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/win10-wpt-content-shell-fyi-rel),
  which runs content shell against `external/wpt/` and `wpt_internal/`
* [`win11-wpt-content-shell-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/win11-wpt-content-shell-fyi-rel),
  which runs content shell against `external/wpt/` and `wpt_internal/`
* [`linux-wpt-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-fyi-rel),
  which runs Chrome against `external/wpt/`
* [`linux-wpt-identity-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-identity-fyi-rel),
  which runs tests under `external/wpt/webauthn/`
* [`linux-wpt-input-fyi-rel`](https://ci.chromium.org/p/chromium/builders/ci/linux-wpt-input-fyi-rel),
  which runs tests under `external/wpt/{input-events,pointerevents,uievents}/`,
  as well as `external/wpt/infrastructure/testdriver/actions/`
* Various
  [Android](https://ci.chromium.org/p/chromium/g/chromium.android.fyi/console)
  builders

Each of these builders has an opt-in trybot mirror with the same name.
To run one of these builders against a CL, click "Choose Tryjobs" in Gerrit,
then search for the builder name.
A [`Cq-Include-Trybots:`](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md#cl-footer-reference)
footer in the CL description can add a `wptrunner` builder to the default CQ
builder set.
Results for the bots use the existing layout test
[results viewer](https://test-results.appspot.com/data/layout_results/linux-wpt-identity-fyi-rel/201/wpt_tests_suite/layout-test-results/results.html).

## Expectations

[Similar to `run_web_tests.py`](web_test_expectations.md), `wptrunner` allows
engineers to specify what results to expect and which tests to skip.
This information is stored in [WPT metadata
files](https://web-platform-tests.org/tools/wptrunner/docs/expectation.html).
Each metadata file is checked in with an `.ini` suffix appended to its
corresponding test file's path:

```
external/wpt/folder/my-test.html
external/wpt/folder/my-test-expected.txt  <-- run_web_tests.py baseline
external/wpt/folder/my-test.html.ini      <-- wptrunner metadata
```

A metadata file is roughly equivalent to a `run_web_tests.py` baseline and the
test's corresponding lines in [web test expectation
files](web_test_expectations.md#Kinds-of-expectations-files).
Metadata files record test and subtest expectations in a structured INI-like
text format:

```
[my-test.html]
  expected: OK
  bug: crbug.com/123  # Comments start with '#'

  [First subtest name (flaky)]
    expected: [PASS, FAIL]  # Expect either a pass or a failure

  [Second subtest name: [\]]  # The backslash escapes a literal ']' in the subtest name
    expected: FAIL
```

The brackets `[...]` denote the start of a (sub)test section, which can be
hierarchically nested with significant indentation.
Each section can contain `<key>: <value>` pairs.
Important keys that `wptrunner` understands:

* `expected`: The
  [statuses](https://firefox-source-docs.mozilla.org/mozbase/mozlog.html#data-format)
  to expect.
    * Tests commonly have these harness statuses: `OK`, `ERROR`, `TIMEOUT`, or
      `CRASH` (for tests without subtests, like reftests, `PASS` replaces `OK`
      and `FAIL` replaces `ERROR`)
    * Subtests commonly have: `PASS`, `FAIL`, or `TIMEOUT`
    * For convenience, `wptrunner` expects `OK` or `PASS` when `expected` is
      omitted.
      Deleting the entire metadata file implies an all-`PASS` test.
* `disabled`: Any nonempty value will disable the test or ignore the subtest
  result.

*** note
**Note**: As shown in the example above, a `testharness.js` test may have a
test-level status of `OK`, even if some subtests `FAIL`. This is a common
point of confusion: `OK` only means that the test ran to completion and did not
`CRASH` or `TIMEOUT`. `OK` does not imply that every subtest `PASS`ed.
***

*** note
**Note**: Currently, `wptrunner` can inherit expectations from
`TestExpectations` files through a [translation
step](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/w3c/wpt_metadata_builder.py).
Due to lost subtest coverage, we are actively working to deprecate this and use
checked-in metadata natively in Chromium.
***

### Conditional Values

`run_web_tests.py` encodes platform- or flag-specific results using [platform
tags](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/port/base.py;l=140-164;drc=023529555939e01068874ddff3a2ea8455125efb;bpv=0;bpt=0)
in test expectations, separate [`FlagExpectations/*`
files](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/FlagExpectations/),
and [baseline fallback](web_test_baseline_fallback.md).
WPT metadata uses a Python-like [conditional
syntax](https://web-platform-tests.org/tools/wptrunner/docs/expectation.html#conditional-values)
instead to store all expectations in one file:

```
[my-test.html]
  expected:
    if not debug: FAIL
    if os == "mac" or (os == "linux" and version != "trusty"): [FAIL, PASS]
    TIMEOUT  # If no branch matches, use this default value.
```

To evaluate a conditional value, `wptrunner` takes the right-hand side of the
first branch where the condition evaluates to a truthy value.
Conditions can contain arbitrary Python-like boolean expressions that will be
evaluated against **properties** (*i.e.*, variables) pulled from the [test
environment](https://firefox-source-docs.mozilla.org/build/buildsystem/mozinfo.html).
Properties available in Chromium are shown below:

| Property | Type | Description | Choices |
| - | - | - | - |
| `os` | `str` | OS family | `linux`, `android` |
| `version` | `str` | OS version | Depends on `os` |
| `product` | `str` | Browser or browser component | `chrome`, `content_shell`, `chrome_android`, `android_webview` |
| `processor` | `str` | CPU specifier | `arm`, `x86`, `x86_64` |
| `flag_specific` | `str` | Flag-specific suite name | See [`FlagSpecificConfig`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/FlagSpecificConfig) (will be falsy for the generic suite) |
| `debug` | `bool` | `is_debug` build? | N/A |

### Test Parameterization

The WPT suite supports forms of test parameterization where a test file on disk
may map to more than one test ID: [multiglobal `.any.js`
tests](https://web-platform-tests.org/writing-tests/testharness.html#tests-for-other-or-multiple-globals-any-js)
and [test
variants](https://web-platform-tests.org/writing-tests/testharness.html#variants).
The metadata for these parameterizations live in the same file (test file path
with the `.ini` suffix), but under different top-level sections.
For example, suppose a test `external/wpt/a.any.js` generates test IDs
`a.any.html?b`, `a.any.html?c`, `a.any.worker.html?b`, and
`a.any.worker.html?c`.
Then, a file named `external/wpt/a.any.js.ini` stores expectations for all
parameterizations:

```
[a.any.html?b]
  expected: OK

[a.any.html?c]
  expected: CRASH

[a.any.worker.html?b]
  expected: TIMEOUT

[a.any.worker.html?c]
  expected: TIMEOUT
```

### Directory-Wide Expectations

To set expectations or disable tests under a directory without editing an `.ini`
file for every test, place a file named `__dir__.ini` under the desired
directory with contents like:

```
expected:
  if os == "linux": CRASH
disabled:
  if flag_specific == "highdpi": skip highdpi for these non-rendering tests
```

Note that there is no section heading `[my-test.html]`, but the keys work
exactly the same as for per-test metadata.

Metadata closer to affected test files take higher precedence.
For example, expectations set by `a/b/test.html.ini` override those of
`a/b/__dir__.ini`, which overrides `a/__dir__.ini`.

The special value `disabled: @False` can selectively reenable child tests or
directories that would have been disabled by a parent `__dir__.ini`.

### Tooling

To help update expectations in bulk,
[`blink_tool.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/tool/blink_tool.py)
has an
[`update-metadata`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/tool/commands/update_metadata.py)
subcommand that can automatically update expectations from try job results
(similar to
[`rebaseline-cl`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/tool/commands/rebaseline_cl.py)).
Example invocation:

``` sh
./blink_tool.py update-metadata --verbose --bug=123 \
    --build=linux-wpt-content-shell-fyi-rel:30 css/
```

This will update the `expected` statuses for `external/wpt/css/` (sub)tests that
ran unexpectedly on [build 30 of
`linux-wpt-content-shell-fyi-rel`](https://ci.chromium.org/ui/p/chromium/builders/try/linux-wpt-content-shell-fyi-rel/30/overview).
Any updated test section will be annotated with `bug: crbug.com/123`.

## Known issues

* There is no debugging support in `run_wpt_tests.py` today. In the future, we
  intend to allow pausing the browser after each test, and (long-term) intend to
  support hooking up `gdb` to test runs.
* There is not yet support for non-Linux platforms. We would love for you to try
  it on other operating systems and file bugs against us if it doesn't work!

Please [file bugs and feature requests](https://crbug.com/new) against
`Blink>Infra`, tagging the title with `[wptrunner]`.
