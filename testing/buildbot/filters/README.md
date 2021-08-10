# Test filter files

## Summary

This directory contains files that list tests that are not yet ready to run in a
particular mode. For example - the `cast-linux.content_browsertests.filter` file
lists tests that should be excluded when running `content_browsertests` on the
Cast device or bot (e.g. on 'Cast Linux).

## File syntax

Contents of test filter files follow the syntax below:

-   Empty lines are ignored.
-   Any text after '#' on each line is treated as comments and ignored.
-   All other lines specify a single [test name pattern][gtest_filter].
    -   Negative patterns: Patterns prefixed with the '-' character specify
        tests to exclude from a test run.
    -   Positive patterns: All other patterns specify tests to include in a test
        run.

A test will be executed if and only if it matches any of the positive patterns
but does not match any of the negative patterns (please see documentation of
[--gtest_filter][gtest_filter] for more details and examples).

Please see [https://crbug.com/587527] for discussion how "positive" and
"negative" test patterns should be combined in presence of both the
`--gtest_filter` and the `--test-launcher-filter-file` command line flags.

Example test filter file for excluding a set of tests:

```test.filter
# crbug.com/417518: Get tests working w/ --site-per-process
-BrowserTest.OtherRedirectsDontForkProcess
-ChromeRenderProcessHostTest.*
-ReferrerPolicyTest.HttpsRedirect

# crbug.com/448592: Get extension browsertests working w/ --site-per-process
-IsolatedAppTest.CookieIsolation
-IsolatedAppTest.CrossProcessClientRedirect
-IsolatedAppTest.IsolatedAppProcessModel
-IsolatedAppTest.SubresourceCookieIsolation
```

## Usage

When running tests on desktop platforms, the test filter file can be specified
using `--test-launcher-filter-file` command line flag. Example test invocation
(single filter file):

```bash
$ out/dbg/content_browsertests \
    --test-launcher-filter-file=testing/buildbot/filters/foo.content_browsertests.filter
```

Example test invocation (multiple filter files, separated by ';'):

```bash
$ out/dbg/content_browsertests \
    --test-launcher-filter-file=testing/buildbot/filters/foo.content_browsertests.filter;\
                                testing/buildbot/filters/foo.chromeos.content_browsertests.filter
```

When running tests on Android, the test filter file can also be specified using
`--test-launcher-filter-file` command line flag. Example test invocation:

```bash
$ out/android/bin/run_content_browsertests \
    --test-launcher-filter-file=testing/buildbot/filters/foo.content_browsertests.filter
```

### Multiple filter files

We are in the process of unifying the way multiple filter files are passed
across all test runners.

Though our standard for passing multiple filter files is to pass each one in its
own flag, we still support ';' for passing multiple filter files on Android for
legacy reasons and haven't implemented multiple filter flags in Gtest. Gtest's
current problematic behavior leads to users passing 2 flags, but only the latter
being respected.


#### Gtest

Multiple filter files should be separated by a ';', passed with one flag.

#### Web tests and Android

Multiple filter files should be passed with multiple filter flags.


## Applicability

Test filter files described here are currently only supported for gtest-based
tests.

For excluding layout tests when running with a particular command line flag, see
`third_party/WebKit/LayoutTests/FlagExpectations/README.txt`.

## Adding new test filter files

Please use the following conventions when naming the new file:

-   Please include the name of the test executable (e.g.
    `content_browsertests`).
-   Please use `.filter` suffix.
-   Feel free to add other relevant things into the file name (e.g. the mode the
    file applies to - for example `site-per-process`).

When adding a new filter file, you will need to:
-   Update `//testing/buildbot/filters/BUILD.gn`.
-   Add `//testing/buildbot/filters:foo_filters` to the appropriate test target.
-   Add `'--test-launcher-filter-file=../../testing/buildbot/filters/foo.filter'`
    to the desired test suite(s) in `test_suites.pyl`.
-   Run `testing/buildbot/generate_buildbot_json.py` to update .json files.

[gtest_filter]: https://github.com/google/googletest/blob/master/docs/advanced.md#running-a-subset-of-the-tests
