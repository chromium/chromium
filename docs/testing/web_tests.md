# Web Tests (formerly known as "Layout Tests" or "LayoutTests")

Web tests are used by Blink to test many components, including but not
limited to layout and rendering. In general, web tests involve loading pages
in a test renderer (`content_shell`) and comparing the rendered output or
JavaScript output against an expected output file.

This document covers running and debugging existing web tests. See the
[Writing Web Tests documentation](./writing_web_tests.md) if you find
yourself writing web tests.

Note that we changed the term "layout tests" to "web tests".
Please assume these terms mean the identical stuff. We also call it as
"WebKit tests" and "WebKit layout tests".

["Web platform tests"](./web_platform_tests.md) (WPT) are the preferred form of
web tests and are located at
[web_tests/external/wpt](/third_party/blink/web_tests/external/wpt).
Tests that should work across browsers go there. Other directories are for
Chrome-specific tests only.

Note: if you are looking for a guide for the Web Platform Test, you should read
["Web platform tests"](./web_platform_tests.md) (WPT). This document does not
cover WPT specific features/behaviors.

Note: if you are looking for a guide for running the Web Platform Tests with
Chrome, Chrome Android or WebView, you should read ["Running Web Platform Tests with run_wpt_tests.py"](./run_web_platform_tests.md).

[TOC]

## Running Web Tests

### Supported Platforms

* Linux
* MacOS
* Windows
* Fuchsia

Android is [not supported](https://crbug.com/567947).

### Initial Setup

Before you can run the web tests, you need to build the `blink_tests` target
to get `content_shell` and all of the other needed binaries.

```bash
autoninja -C out/Default blink_tests
```

On **Mac**, you probably want to strip the content_shell binary before starting
the tests. If you don't, you'll have 5-10 running concurrently, all stuck being
examined by the OS crash reporter. This may cause other failures like timeouts
where they normally don't occur.

```bash
strip ./out/Default/Content\ Shell.app/Contents/MacOS/Content\ Shell
```

### Running the Tests

The test runner script is in `third_party/blink/tools/run_web_tests.py`.

To specify which build directory to use (e.g. out/Default, etc.)
you should pass the `-t` or `--target` parameter. If no directory is specified,
`out/Release` will be used. To use the built-in `out/Default`, use:

```bash
third_party/blink/tools/run_web_tests.py -t Default
```

*** promo
* Windows users need to use `third_party\blink\tools\run_web_tests.bat` instead.
* Linux users should not use `testing/xvfb.py`; `run_web_tests.py` manages Xvfb
  itself.
***

Tests marked as `[ Skip ]` in
[TestExpectations](../../third_party/blink/web_tests/TestExpectations)
won't be run by default, generally because they cause some intractable tool error.
To force one of them to be run, either rename that file or specify the skipped
test on the command line (see below) or in a file specified with --test-list
(however, --skip=always can make the tests marked as `[ Skip ]` always skipped).
Read the [Web Test Expectations documentation](./web_test_expectations.md) to
learn more about TestExpectations and related files.

*** promo
Currently only the tests listed in
[Default.txt](../../third_party/blink/web_tests/TestLists/Default.txt) are run
on the Fuchsia bots, since running all web tests takes too long on Fuchshia.
Most developers focus their Blink testing on Linux. We rely on the fact that the
Linux and Fuchsia behavior is nearly identical for scenarios outside those
covered by the smoke tests.
***

*** promo
Similar to Fuchsia's case, the tests listed in [Mac.txt]
(../../third_party/blink/web_tests/TestLists/Mac.txt)
are run on older mac version bots. By doing this we reduced the resources needed to run
the tests. This relies on the fact that the majority of web tests will behavior similarly on
different mac versions.
***

To run only some of the tests, specify their directories or filenames as
arguments to `run_web_tests.py` relative to the web test directory
(`src/third_party/blink/web_tests`). For example, to run the fast form tests,
use:

```bash
third_party/blink/tools/run_web_tests.py fast/forms
```

Or you could use the following shorthand:

```bash
third_party/blink/tools/run_web_tests.py fast/fo\*
```

*** promo
Example: To run the web tests with a debug build of `content_shell`, but only
test the SVG tests and run pixel tests, you would run:

```bash
third_party/blink/tools/run_web_tests.py -t Default svg
```
***

As a final quick-but-less-robust alternative, you can also just use the
content_shell executable to run specific tests by using (example on Windows):

```bash
out\Default\content_shell.exe --run-web-tests <url>|<full_test_source_path>|<relative_test_path>
```

as in:

```bash
out\Default\content_shell.exe --run-web-tests \
    c:\chrome\src\third_party\blink\web_tests\fast\forms\001.html
```
or

```bash
out\Default\content_shell.exe --run-web-tests fast\forms\001.html
```

but this requires a manual diff against expected results, because the shell
doesn't do it for you. It also just dumps the text result only (as the dump of
pixels and audio binary data is not human readable).
See [Running Web Tests Using the Content Shell](./web_tests_in_content_shell.md)
for more details of running `content_shell`.

To see a complete list of arguments supported, run:

```bash
third_party/blink/tools/run_web_tests.py --help
```

*** note
**Linux Note:** We try to match the Windows render tree output exactly by
matching font metrics and widget metrics. If there's a difference in the render
tree output, we should see if we can avoid rebaselining by improving our font
metrics. For additional information on Linux web tests, please see
[docs/web_tests_linux.md](./web_tests_linux.md).
***

*** note
**Mac Note:** While the tests are running, a bunch of Appearance settings are
overridden for you so the right type of scroll bars, colors, etc. are used.
Your main display's "Color Profile" is also changed to make sure color
correction by ColorSync matches what is expected in the pixel tests. The change
is noticeable, how much depends on the normal level of correction for your
display. The tests do their best to restore your setting when done, but if
you're left in the wrong state, you can manually reset it by going to
System Preferences → Displays → Color and selecting the "right" value.
***

### Test Harness Options

This script has a lot of command line flags. You can pass `--help` to the script
to see a full list of options. A few of the most useful options are below:

| Option                      | Meaning |
|:----------------------------|:--------------------------------------------------|
| `--debug`                   | Run the debug build of the test shell (default is release). Equivalent to `-t Debug` |
| `--nocheck-sys-deps`        | Don't check system dependencies; this allows faster iteration. |
| `--verbose`                 |	Produce more verbose output, including a list of tests that pass. |
| `--reset-results`           |	Overwrite the current baselines (`-expected.{png`&#124;`txt`&#124;`wav}` files) with actual results, or create new baselines if there are no existing baselines. |
| `--fully-parallel`          | Run tests in parallel using as many child processes as the system has cores. |
| `--driver-logging`          | Print C++ logs (LOG(WARNING), etc).  |

## Success and Failure

A test succeeds when its output matches the pre-defined expected results. If any
tests fail, the test script will place the actual generated results, along with
a diff of the actual and expected results, into
`src/out/Default/layout-test-results/`, and by default launch a browser with a
summary and link to the results/diffs.

The expected results for tests are in the
`src/third_party/blink/web_tests/platform` or alongside their respective
tests.

*** note
Tests which use [testharness.js](https://github.com/w3c/testharness.js/)
do not have expected result files if all test cases pass.
***

A test that runs but produces the wrong output is marked as "failed", one that
causes the test shell to crash is marked as "crashed", and one that takes longer
than a certain amount of time to complete is aborted and marked as "timed out".
A row of dots in the script's output indicates one or more tests that passed.

## Test expectations

The
[TestExpectations](../../third_party/blink/web_tests/TestExpectations) file (and related
files) contains the list of all known web test failures. See the
[Web Test Expectations documentation](./web_test_expectations.md) for more
on this.

## Testing Runtime Flags

There are two ways to run web tests with additional command-line arguments:

### --flag-specific

```bash
third_party/blink/tools/run_web_tests.py --flag-specific=blocking-repaint
```
It requires that `web_tests/FlagSpecificConfig` contains an entry like:

```json
{
  "name": "blocking-repaint",
  "args": ["--blocking-repaint", "--another-flag"]
}
```

This tells the test harness to pass `--blocking-repaint --another-flag` to the
content_shell binary.

It will also look for flag-specific expectations in
`web_tests/FlagExpectations/blocking-repaint`, if this file exists. The
suppressions in this file override the main TestExpectations files.
However, `[ Slow ]` in either flag-specific expectations or base expectations
is always merged into the used expectations.

It will also look for baselines in `web_tests/flag-specific/blocking-repaint`.
The baselines in this directory override the fallback baselines.

*** note
[BUILD.gn](../../BUILD.gn) assumes flag-specific builders always runs on linux bots, so
flag-specific test expectations and baselines are only downloaded to linux bots.
If you need run flag-specific builders on other platforms, please update
BUILD.gn to download flag-specific related data to that platform.
***

You can also use `--additional-driver-flag` to specify additional command-line
arguments to content_shell, but the test harness won't use any flag-specific
test expectations or baselines.

### Virtual test suites

A *virtual test suite* can be defined in
[web_tests/VirtualTestSuites](../../third_party/blink/web_tests/VirtualTestSuites),
to run a subset of web tests with additional flags, with
`virtual/<prefix>/...` in their paths. The tests can be virtual tests that
map to real base tests (directories or files) whose paths match any of the
specified bases, or any real tests under `web_tests/virtual/<prefix>/`
directory. For example, you could test a (hypothetical) new mode for
repainting using the following virtual test suite:

```json
{
  "prefix": "blocking_repaint",
  "platforms": ["Linux", "Mac", "Win"],
  "bases": ["compositing", "fast/repaint"],
  "args": ["--blocking-repaint"]
}
```

This will create new "virtual" tests of the form
`virtual/blocking_repaint/compositing/...` and
`virtual/blocking_repaint/fast/repaint/...` which correspond to the files
under `web_tests/compositing` and `web_tests/fast/repaint`, respectively,
and pass `--blocking-repaint` to `content_shell` when they are run.

Note that you can run the tests with the following command line:

```bash
third_party/blink/tools/run_web_tests.py virtual/blocking_repaint/compositing \
  virtual/blocking_repaint/fast/repaint
```

These virtual tests exist in addition to the original `compositing/...` and
`fast/repaint/...` tests. They can have their own expectations in
`web_tests/TestExpectations`, and their own baselines. The test harness will
use the non-virtual expectations and baselines as a fallback. If a virtual
test has its own expectations, they will override all non-virtual
expectations. Otherwise the non-virtual expectations will be used. However,
`[ Slow ]` in either virtual or non-virtual expectations is always merged
into the used expectations. If a virtual test is expected to pass while the
non-virtual test is expected to fail, you need to add an explicit `[ Pass ]`
entry for the virtual test.

This will also let any real tests under `web_tests/virtual/blocking_repaint`
directory run with the `--blocking-repaint` flag.

The "platforms" configuration can be used to skip tests on some platforms. If
a virtual test suites uses more than 5% of total test time, we should consider
to skip the test suites on some platforms.

The "prefix" value should be unique. Multiple directories with the same flags
should be listed in the same "bases" list. The "bases" list can be empty,
in case that we just want to run the real tests under `virtual/<prefix>`
with the flags without creating any virtual tests.

A virtual test suite can have an optional `exclusive_tests` field to specify
all (with `"ALL"`) or a subset of `bases` tests that will be exclusively run
under this virtual suite. The specified base tests will be skipped. Corresponding
virtual tests under other virtual suites that don't specify the tests in their
`exclusive_tests` list will be skipped, too. For example (unrelated fields
are omitted):

```json
{
  "prefix": "v1",
  "bases": ["a"],
}
{
  "prefix": "v2",
  "bases": ["a/a1", "a/a2"],
  "exclusive_tests": "ALL",
}
{
  "prefix": "v3",
  "bases": ["a"],
  "exclusive_tests": ["a/a1"],
}
```

Suppose there are directories `a/a1`, `a/a2` and `a/a3`, we will run the
following tests:

|      Suite |   a/a1  |   a/a2  | a/a3 |
| ---------: | :-----: | :-----: | :--: |
|       base | skipped | skipped | run  |
| virtual/v1 | skipped | skipped | run  |
| virtual/v2 |   run   |   run   | n/a  |
| virtual/v3 |   run   | skipped | run  |

In a similar manner, a virtual test suite can also have an optional
`skip_base_tests` field to specify all (with `"ALL"`) or a subset of `bases`
tests that will be run under this virtual while the base tests will be skipped.
This will not affect other virtual suites.

```json
{
  "prefix": "v1",
  "bases": ["a/a1"],
}
{
  "prefix": "v2",
  "bases": ["a/a1"],
  "skip_base_tests": "ALL",
}
```
Suppose there are directories `a/a1` and `a/a2` we will run the following tests:

|      Suite |   a/a1  |   a/a2  |
| ---------: | :-----: | :-----: |
|       base | skipped |   run   |
| virtual/v1 |   run   |   n/a   |
| virtual/v2 |   run   |   n/a   |


### Choosing between flag-specific and virtual test suite

For flags whose implementation is still in progress, flag-specific expectations
and virtual test suites represent two alternative strategies for testing both
the enabled code path and non-enabled code path. They are preferred to only
setting a [runtime enabled feature](../../third_party/blink/renderer/platform/RuntimeEnabledFeatures.md)
to `status: "test"` if the feature has substantially different code path from
production because the latter would cause loss of test coverage of the production
code path.

Consider the following when choosing between virtual test suites and
flag-specific suites:

* The
  [waterfall builders](https://dev.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot)
  and [try bots](https://dev.chromium.org/developers/testing/try-server-usage)
  will run all virtual test suites in addition to the non-virtual tests.
  Conversely, a flag-specific configuration won't automatically cause the bots
  to test your flag - if you want bot coverage without virtual test suites, you
  will need to follow [these instructions](#running-a-new-flag_specific-suite-in-cq_ci).

* Due to the above, virtual test suites incur a performance penalty for the
  commit queue and the continuous build infrastructure. This is exacerbated by
  the need to restart `content_shell` whenever flags change, which limits
  parallelism. Therefore, you should avoid adding large numbers of virtual test
  suites. They are well suited to running a subset of tests that are directly
  related to the feature, but they don't scale to flags that make deep
  architectural changes that potentially impact all of the tests.

* Note that using wildcards in virtual test path names (e.g.
  `virtual/blocking_repaint/fast/repaint/*`) is not supported in
  `run_web_tests.py` command line , but you can still use
  `virtual/blocking_repaint` to run all real and virtual tests
  in the suite or `virtual/blocking_repaint/fast/repaint/dir` to run real
  or virtual tests in the suite under a specific directory.

*** note
We can run a virtual test with additional flags. Both the virtual args and the
additional flags will be applied. The fallback order of baselines and
expectations will be: 1) flag-specific virtual, 2) non-flag-specific virtual,
3) flag-specific base, 4) non-flag-specific base
***

### Running a New Flag-Specific Suite in CQ/CI

Assuming you have already created a `FlagSpecificConfig` entry:

1. File a resource request ([internal
   docs](https://g3doc.corp.google.com/company/teams/chrome/ops/business/resources/resource-request-program.md?cl=head&polyglot=chrome-browser#i-need-new-resources))
   for increased capacity in the `chromium.tests` swarming pool and wait for
   approval.
1. Define a new dedicated
   [Buildbot test suite](https://source.chromium.org/chromium/chromium/src/+/main:testing/buildbot/test_suites.pyl;l=1516-1583;drc=0694b605fb77c975a065a3734bdcf3bd81fd8ca4;bpv=0;bpt=0)
   with `--flag-specific` and possibly other special configurations (e.g., fewer shards).
1. Add the Buildbot suite to the relevant `*-blink-rel` builder's
   composition suite first
   ([example](https://source.chromium.org/chromium/chromium/src/+/main:testing/buildbot/test_suites.pyl;l=5779-5780;drc=0694b605fb77c975a065a3734bdcf3bd81fd8ca4;bpv=0;bpt=0)).
1. Add the flag-specific step name to the relevant builder in
   [`builders.json`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/common/config/builders.json;l=127-129;drc=ff938aaff9566b2cc442476a51835e0b90b1c6f6;bpv=0;bpt=0).
   `rebaseline-cl` and the WPT importer will now create baselines for that suite.
1. Rebaseline the new suite and add any necessary suppressions under
   `FlagExpectations/`.
1. Enable the flag-specific suite for CQ/CI by adding the Buildbot suite to the
   desired builder.
   This could be an existing CQ builder like
   [`linux-rel`](https://source.chromium.org/chromium/chromium/src/+/main:testing/buildbot/test_suites.pyl;l=5828-5829;drc=0694b605fb77c975a065a3734bdcf3bd81fd8ca4;bpv=0;bpt=0)
   or a dedicated builder like
   [`linux-blink-web-tests-force-accessibility-rel`](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/subprojects/chromium/try/tryserver.chromium.accessibility.star;drc=adad4c6d55e69783ba1f16d30f4bc7367e2e626a;bpv=0;bpt=0), which has customized location filters.

## Tracking Test Failures

All bugs, associated with web test failures must have the
[Test-Layout](https://crbug.com/?q=label:Test-Layout) label. Depending on how
much you know about the bug, assign the status accordingly:

* **Unconfirmed** -- You aren't sure if this is a simple rebaseline, possible
  duplicate of an existing bug, or a real failure
* **Untriaged** -- Confirmed but unsure of priority or root cause.
* **Available** -- You know the root cause of the issue.
* **Assigned** or **Started** -- You will fix this issue.

When creating a new web test bug, please set the following properties:

* Components: a sub-component of Blink
* OS: **All** (or whichever OS the failure is on)
* Priority: 2 (1 if it's a crash)
* Type: **Bug**
* Labels: **Test-Layout**

You can also use the _Layout Test Failure_ template, which pre-sets these
labels for you.

## Debugging Web Tests

After the web tests run, you should get a summary of tests that pass or
fail. If something fails unexpectedly (a new regression), you will get a
`content_shell` window with a summary of the unexpected failures. Or you might
have a failing test in mind to investigate. In any case, here are some steps and
tips for finding the problem.

* Take a look at the result. Sometimes tests just need to be rebaselined (see
  below) to account for changes introduced in your patch.
    * Load the test into a trunk Chrome or content_shell build and look at its
      result. (For tests in the http/ directory, start the http server first.
      See above. Navigate to `http://localhost:8000/` and proceed from there.)
      The best tests describe what they're looking for, but not all do, and
      sometimes things they're not explicitly testing are still broken. Compare
      it to Safari, Firefox, and IE if necessary to see if it's correct. If
      you're still not sure, find the person who knows the most about it and
      ask.
    * Some tests only work properly in content_shell, not Chrome, because they
      rely on extra APIs exposed there.
    * Some tests only work properly when they're run in the web-test
      framework, not when they're loaded into content_shell directly. The test
      should mention that in its visible text, but not all do. So try that too.
      See "Running the tests", above.
* If you think the test is correct, confirm your suspicion by looking at the
  diffs between the expected result and the actual one.
    * Make sure that the diffs reported aren't important. Small differences in
      spacing or box sizes are often unimportant, especially around fonts and
      form controls. Differences in wording of JS error messages are also
      usually acceptable.
    * `third_party/blink/tools/run_web_tests.py path/to/your/test.html` produces
      a page listing all test results. Those which fail their expectations will
      include links to the expected result, actual result, and diff. These
      results are saved to `$root_build_dir/layout-test-results`.
        * Alternatively the `--results-directory=path/for/output/` option allows
          you to specify an alternative directory for the output to be saved to.
    * If you're still sure it's correct, rebaseline the test (see below).
      Otherwise...
* If you're lucky, your test is one that runs properly when you navigate to it
  in content_shell normally. In that case, build the Debug content_shell
  project, fire it up in your favorite debugger, and load the test file either
  from a `file:` URL.
    * You'll probably be starting and stopping the content_shell a lot. In VS,
      to save navigating to the test every time, you can set the URL to your
      test (`file:` or `http:`) as the command argument in the Debugging section of
      the content_shell project Properties.
    * If your test contains a JS call, DOM manipulation, or other distinctive
      piece of code that you think is failing, search for that in the Chrome
      solution. That's a good place to put a starting breakpoint to start
      tracking down the issue.
    * Otherwise, you're running in a standard message loop just like in Chrome.
      If you have no other information, set a breakpoint on page load.
* If your test only works in full web-test mode, or if you find it simpler to
  debug without all the overhead of an interactive session, start the
  content_shell with the command-line flag `--run-web-tests`, followed by the
  URL (`file:` or `http:`) to your test. More information about running web tests
  in content_shell can be found [here](./web_tests_in_content_shell.md).
    * In VS, you can do this in the Debugging section of the content_shell
      project Properties.
    * Now you're running with exactly the same API, theme, and other setup that
      the web tests use.
    * Again, if your test contains a JS call, DOM manipulation, or other
      distinctive piece of code that you think is failing, search for that in
      the Chrome solution. That's a good place to put a starting breakpoint to
      start tracking down the issue.
    * If you can't find any better place to set a breakpoint, start at the
      `TestShell::RunFileTest()` call in `content_shell_main.cc`, or at
      `shell->LoadURL() within RunFileTest()` in `content_shell_win.cc`.
* Debug as usual. Once you've gotten this far, the failing web test is just a
  (hopefully) reduced test case that exposes a problem.

### Debugging HTTP Tests

Note: HTTP Tests mean tests under `web_tests/http/tests/`,
which is a subset of WebKit Layout Tests originated suite.
If you want to debug WPT's HTTP behavior, you should read
["Web platform tests"](./web_platform_tests.md) instead.


To run the server manually to reproduce/debug a failure:

```bash
third_party/blink/tools/run_blink_httpd.py
```

The web tests are served from `http://127.0.0.1:8000/`. For example, to
run the test
`web_tests/http/tests/serviceworker/chromium/service-worker-allowed.html`,
navigate to
`http://127.0.0.1:8000/serviceworker/chromium/service-worker-allowed.html`. Some
tests behave differently if you go to `127.0.0.1` vs. `localhost`, so use
`127.0.0.1`.

To kill the server, hit any key on the terminal where `run_blink_httpd.py` is
running, use `taskkill` or the Task Manager on Windows, or `killall` or
Activity Monitor on macOS.

The test server sets up an alias to the `web_tests/resources` directory. For
example, in HTTP tests, you can access the testing framework using
`src="/js-test-resources/js-test.js"`.

### Tips

Check https://test-results.appspot.com/ to see how a test did in the most recent
~100 builds on each builder (as long as the page is being updated regularly).

A timeout will often also be a text mismatch, since the wrapper script kills the
content_shell before it has a chance to finish. The exception is if the test
finishes loading properly, but somehow hangs before it outputs the bit of text
that tells the wrapper it's done.

Why might a test fail (or crash, or timeout) on buildbot, but pass on your local
machine?
* If the test finishes locally but is slow, more than 10 seconds or so, that
  would be why it's called a timeout on the bot.
* Otherwise, try running it as part of a set of tests; it's possible that a test
  one or two (or ten) before this one is corrupting something that makes this
  one fail.
* If it consistently works locally, make sure your environment looks like the
  one on the bot (look at the top of the stdio for the webkit_tests step to see
  all the environment variables and so on).
* If none of that helps, and you have access to the bot itself, you may have to
  log in there and see if you can reproduce the problem manually.

### Debugging DevTools Tests

* Do one of the following:
    * Option A) Run from the `chromium/src` folder:
      `third_party/blink/tools/run_web_tests.py --additional-driver-flag='--remote-debugging-port=9222' --additional-driver-flag='--remote-allow-origins=*' --additional-driver-flag='--debug-devtools' --timeout-ms=6000000`
    * Option B) If you need to debug an http/tests/inspector test, start httpd
      as described above. Then, run content_shell:
      `out/Default/content_shell --remote-debugging-port=9222 --additional-driver-flag='--remote-allow-origins=*' --additional-driver-flag='--debug-devtools' --run-web-tests http://127.0.0.1:8000/path/to/test.html`
* Open `http://localhost:9222` in a stable/beta/canary Chrome, click the single
  link to open the devtools with the test loaded.
* In the loaded devtools, set any required breakpoints and execute `test()` in
  the console to actually start the test.

NOTE: If the test is an html file, this means it's a legacy test so you need to add:
* Add `window.debugTest = true;` to your test code as follows:

  ```javascript
  window.debugTest = true;
  function test() {
    /* TEST CODE */
  }
  ```

### Reproducing flaky inspector protocol tests

https://crrev.com/c/5318502 implemented logging for inspector-protocol tests.
With this CL for each test in stderr you should see Chrome DevTools Protocol
messages that the test and the browser exchanged.

You can use this log to reproduce the failure or timeout locally.

* Prepare a log file and ensure each line contains one protocol message
in the JSON format. Strip any prefixes or non-protocol messages from the
original log.
* Make sure your local test file version matches the version that produced
the log file.
* Run the test using the log file:

  ```sh
  third_party/blink/tools/run_web_tests.py -t Release \
   --additional-driver-flag="--inspector-protocol-log=/path/to/log.txt" \
   http/tests/inspector-protocol/network/url-fragment.js
  ```

## Bisecting Regressions

You can use [`git bisect`](https://git-scm.com/docs/git-bisect) to find which
commit broke (or fixed!) a web test in a fully automated way.  Unlike
[bisect-builds.py](http://dev.chromium.org/developers/bisect-builds-py), which
downloads pre-built Chromium binaries, `git bisect` operates on your local
checkout, so it can run tests with `content_shell`.

Bisecting can take several hours, but since it is fully automated you can leave
it running overnight and view the results the next day.

To set up an automated bisect of a web test regression, create a script like
this:

```bash
#!/bin/bash

# Exit code 125 tells git bisect to skip the revision.
gclient sync || exit 125
autoninja -C out/Debug -j100 blink_tests || exit 125

third_party/blink/tools/run_web_tests.py -t Debug \
  --no-show-results --no-retry-failures \
  path/to/web/test.html
```

Modify the `out` directory, ninja args, and test name as appropriate, and save
the script in `~/checkrev.sh`.  Then run:

```bash
chmod u+x ~/checkrev.sh  # mark script as executable
git bisect start <badrev> <goodrev>
git bisect run ~/checkrev.sh
git bisect reset  # quit the bisect session
```

## Rebaselining Web Tests

See [How to rebaseline](./web_test_expectations.md#How-to-rebaseline).

## Known Issues

See
[bugs with the component Blink>Infra](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3ABlink%3EInfra)
for issues related to Blink tools, include the web test runner.

* If QuickTime is not installed, the plugin tests
  `fast/dom/object-embed-plugin-scripting.html` and
  `plugins/embed-attributes-setting.html` are expected to fail.
