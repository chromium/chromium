# Web Test Expectations and Baselines


The primary function of the web tests is as a regression test suite; this
means that, while we care about whether a page is being rendered correctly, we
care more about whether the page is being rendered the way we expect it to. In
other words, we look more for changes in behavior than we do for correctness.

[TOC]

All web tests have "expected results", or "baselines", which may be one of
several forms. The test may produce one or more of:

* A text file containing JavaScript log messages.
* A text rendering of the Render Tree.
* A screen capture of the rendered page as a PNG file.
* WAV files of the audio output, for WebAudio tests.

For any of these types of tests, baselines are checked into the web_tests
directory. The filename of a baseline is the same as that of the corresponding
test, but the extension is replaced with `-expected.{txt,png,wav}` (depending on
the type of test output). Baselines usually live alongside tests, with the
exception when baselines vary by platforms; read
[Web Test Baseline Fallback](web_test_baseline_fallback.md) for more
details.

Lastly, we also support the concept of "reference tests", which check that two
pages are rendered identically (pixel-by-pixel). As long as the two tests'
output match, the tests pass. For more on reference tests, see
[Writing ref tests](https://trac.webkit.org/wiki/Writing%20Reftests).

## Failing tests

When the output doesn't match, there are two potential reasons for it:

* The port is performing "correctly", but the output simply won't match the
  generic version. The usual reason for this is for things like form controls,
  which are rendered differently on each platform.
* The port is performing "incorrectly" (i.e., the test is failing).

In both cases, the convention is to check in a new baseline (aka rebaseline),
even though that file may be codifying errors. This helps us maintain test
coverage for all the other things the test is testing while we resolve the bug.

*** promo
If a test can be rebaselined, it should always be rebaselined instead of adding
lines to TestExpectations.
***

Bugs at [crbug.com](https://crbug.com) should track fixing incorrect behavior,
not lines in
[TestExpectations](../../third_party/blink/web_tests/TestExpectations). If a
test is never supposed to pass (e.g. it's testing Windows-specific behavior, so
can't ever pass on Linux/Mac), move it to the
[NeverFixTests](../../third_party/blink/web_tests/NeverFixTests) file. That
gets it out of the way of the rest of the project.

There are some cases where you can't rebaseline and, unfortunately, we don't
have a better solution than either:

1. Reverting the patch that caused the failure, or
2. Adding a line to TestExpectations and fixing the bug later.

In this case, **reverting the patch is strongly preferred**.

These are the cases where you can't rebaseline:

* The test is a reference test.
* The test gives different output in release and debug; in this case, generate a
  baseline with the release build, and mark the debug build as expected to fail.
* The test is flaky, crashes or times out.
* The test is for a feature that hasn't yet shipped on some platforms yet, but
  will shortly.

## Handling flaky tests

The
[flakiness dashboard](https://test-results.appspot.com/dashboards/flakiness_dashboard.html)
is a tool for understanding a test’s behavior over time.
Originally designed for managing flaky tests, the dashboard shows a timeline
view of the test’s behavior over time. The tool may be overwhelming at first,
but
[the documentation](https://dev.chromium.org/developers/testing/flakiness-dashboard)
should help. Once you decide that a test is truly flaky, you can suppress it
using the TestExpectations file, as described below.

We do not generally expect Chromium sheriffs to spend time trying to address
flakiness, though.

## How to rebaseline

Since baselines themselves are often platform-specific, updating baselines in
general requires fetching new test results after running the test on multiple
platforms.

### Rebaselining using try jobs

The recommended way to rebaseline for a currently-in-progress CL is to use
results from try jobs, by using the command-tool
`third_party/blink/tools/blink_tool.py rebaseline-cl`:

1. First, upload a CL.
2. Trigger try jobs by running `blink_tool.py rebaseline-cl`. This should
   trigger jobs on
   [tryserver.blink](https://build.chromium.org/p/tryserver.blink/builders).
3. Wait for all try jobs to finish.
4. Run `blink_tool.py rebaseline-cl` again to fetch new baselines.
   By default, this will download new baselines for any failing tests
   in the try jobs.
   (Run `blink_tool.py rebaseline-cl --help` for more specific options.)
5. Commit the new baselines and upload a new patch.

This way, the new baselines can be reviewed along with the changes, which helps
the reviewer verify that the new baselines are correct. It also means that there
is no period of time when the web test results are ignored.

#### Options

### Rebaselining with try jobs

The tests which `blink_tool.py rebaseline-cl` tries to download new baselines for
depends on its arguments.

* By default, it tries to download all baselines for tests that failed in the
  try jobs.
* If you pass `--only-changed-tests`, then only tests modified in the CL will be
  considered.
* You can also explicitly pass a list of test names, and then just those tests
  will be rebaselined.
* If some of the try jobs failed to run, and you wish to continue rebaselining
  assuming that there are no platform-specific results for those platforms,
  you can add the flag `--fill-missing`.

## Kinds of expectations files

* [TestExpectations](../../third_party/blink/web_tests/TestExpectations): The
  main test failure suppression file. In theory, this should be used for
  temporarily marking tests as flaky.
* [ASANExpectations](../../third_party/blink/web_tests/ASANExpectations):
  Tests that fail under ASAN.
* [LeakExpectations](../../third_party/blink/web_tests/LeakExpectations):
  Tests that have memory leaks under the leak checker.
* [MSANExpectations](../../third_party/blink/web_tests/MSANExpectations):
  Tests that fail under MSAN.
* [NeverFixTests](../../third_party/blink/web_tests/NeverFixTests): Tests
  that we never intend to fix (e.g. a test for Windows-specific behavior will
  never be fixed on Linux/Mac). Tests that will never pass on any platform
  should just be deleted, though.
* [SlowTests](../../third_party/blink/web_tests/SlowTests): Tests that take
  longer than the usual timeout to run. Slow tests are given 5x the usual
  timeout.
* [SmokeTests](../../third_party/blink/web_tests/SmokeTests): A small subset
  of tests that we run on the Android bot.
* [StaleTestExpectations](../../third_party/blink/web_tests/StaleTestExpectations):
  Platform-specific lines that have been in TestExpectations for many months.
  They're moved here to get them out of the way of people doing rebaselines
  since they're clearly not getting fixed anytime soon.
* [W3CImportExpectations](../../third_party/blink/web_tests/W3CImportExpectations):
  A record of which W3C tests should be imported or skipped.

### Flag-specific expectations files

It is possible to handle tests that only fail when run with a particular flag
being passed to `content_shell`. See
[web_tests/FlagExpectations/README.txt](../../third_party/blink/web_tests/FlagExpectations/README.txt)
for more.

## Updating the expectations files

### Ordering

The file is not ordered. If you put new changes somewhere in the middle of the
file, this will reduce the chance of merge conflicts when landing your patch.

### Syntax

The syntax of the file is roughly one expectation per line. An expectation can
apply to either a directory of tests, or a specific tests. Lines prefixed with
`# ` are treated as comments, and blank lines are allowed as well.

The syntax of a line is roughly:

```
[ bugs ] [ "[" modifiers "]" ] test_name [ "[" expectations "]" ]
```

* Tokens are separated by whitespace.
* **The brackets delimiting the modifiers and expectations from the bugs and the
  test_name are not optional**; however the modifiers component is optional. In
  other words, if you want to specify modifiers or expectations, you must
  enclose them in brackets.
* Lines are expected to have one or more bug identifiers, and the linter will
  complain about lines missing them. Bug identifiers are of the form
  `crbug.com/12345`, `code.google.com/p/v8/issues/detail?id=12345` or
  `Bug(username)`.
* If no modifiers are specified, the test applies to all of the configurations
  applicable to that file.
* Modifiers can be one or more of `Mac`, `Mac10.9`, `Mac10.10`, `Mac10.11`,
  `Retina`, `Win`, `Win7`, `Win10`, `Linux`, `Linux32`, `Precise`, `Trusty`,
  `Android`, `Release`, `Debug`.
* Some modifiers are meta keywords, e.g. `Win` represents both `Win7` and
  `Win10`. See the `CONFIGURATION_SPECIFIER_MACROS` dictionary in
  [third_party/blink/tools/blinkpy/web_tests/port/base.py](../../third_party/blink/tools/blinkpy/web_tests/port/base.py)
  for the meta keywords and which modifiers they represent.
* Expectations can be one or more of `Crash`, `Failure`, `Pass`, `Rebaseline`,
  `Slow`, `Skip`, `Timeout`, `WontFix`, `Missing`.
  If multiple expectations are listed, the test is considered "flaky" and any
  of those results will be considered as expected.

For example:

```
crbug.com/12345 [ Win Debug ] fast/html/keygen.html [ Crash ]
```

which indicates that the "fast/html/keygen.html" test file is expected to crash
when run in the Debug configuration on Windows, and the tracking bug for this
crash is bug \#12345 in the [Chromium issue tracker](https://crbug.com). Note
that the test will still be run, so that we can notice if it doesn't actually
crash.

Assuming you're running a debug build on Mac 10.9, the following lines are all
equivalent (in terms of whether the test is performed and its expected outcome):

```
fast/html/keygen.html [ Skip ]
fast/html/keygen.html [ WontFix ]
Bug(darin) [ Mac10.9 Debug ] fast/html/keygen.html [ Skip ]
```

### Semantics

* `WontFix` implies `Skip` and also indicates that we don't have any plans to
  make the test pass.
* `WontFix` lines always go in the
  [NeverFixTests file](../../third_party/blink/web_tests/NeverFixTests) as
  we never intend to fix them. These are just for tests that only apply to some
  subset of the platforms we support.
* `WontFix` and `Skip` must be used by themselves and cannot be specified
  alongside `Crash` or another expectation keyword.
* `Slow` causes the test runner to give the test 5x the usual time limit to run.
  `Slow` lines go in the
  [SlowTests file ](../../third_party/blink/web_tests/SlowTests). A given
  line cannot have both Slow and Timeout.

Also, when parsing the file, we use two rules to figure out if an expectation
line applies to the current run:

1. If the configuration parameters don't match the configuration of the current
   run, the expectation is ignored.
2. Expectations that match more of a test name are used before expectations that
   match less of a test name.

For example, if you had the following lines in your file, and you were running a
debug build on `Mac10.10`:

```
crbug.com/12345 [ Mac10.10 ] fast/html [ Failure ]
crbug.com/12345 [ Mac10.10 ] fast/html/keygen.html [ Pass ]
crbug.com/12345 [ Win7 ] fast/forms/submit.html [ Failure ]
crbug.com/12345 fast/html/section-element.html [ Failure Crash ]
```

You would expect:

* `fast/html/article-element.html` to fail with a text diff (since it is in the
  fast/html directory).
* `fast/html/keygen.html` to pass (since the exact match on the test name).
* `fast/forms/submit.html` to pass (since the configuration parameters don't
  match).
* `fast/html/section-element.html` to either crash or produce a text (or image
  and text) failure, but not time out or pass.

*** promo
Duplicate expectations are not allowed within the file and will generate
warnings.
***

You can verify that any changes you've made to an expectations file are correct
by running:

```bash
third_party/blink/tools/lint_test_expectations.py
```

which will cycle through all of the possible combinations of configurations
looking for problems.
