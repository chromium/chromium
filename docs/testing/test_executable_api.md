# The Chromium Test Executable API

[bit.ly/chromium-test-runner-api][1] (*)


[TOC]

## Introduction

This document defines the API that test executables must implement in order to
be run on the Chromium continuous integration infrastructure (the
[LUCI][2]
system using the `chromium` and `chromium_trybot` recipes).

*** note
**NOTE:** This document specifies the existing `isolated_scripts` API in the
Chromium recipe. Currently we also support other APIs (e.g., for
GTests), but we should migrate them to use the `isolated_scripts` API.
That work is not currently scheduled.
***

This spec applies only to functional tests and does not attempt to
specify how performance tests should work, though in principle they
could probably work the same way and possibly just produce different
output.

This document is specifically targeted at Chromium and assumes you are
using GN and Ninja for your build system. It should be possible to adapt
these APIs to other projects and build recipes, but this is not an
immediate goal. Similarly, if a project adapts this API and the related
specifications it should be able to reuse the functionality and tooling
we've built out for Chromium's CI system more easily in other LUCI
deployments.

***
**NOTE:** It bears repeating that this describes the current state of
affairs, and not the desired end state. A companion doc,
[Cleaning up the Chromium Testing Environment][3],
discusses a possible path forward and end state.
***

## Building and Invoking a Test Executable

There are lots of different kinds of tests, but we want to be able to
build and invoke them uniformly, regardless of how they are implemented.

We will call the thing being executed to run the tests a _test
executable_ (or executable for short). This is not an ideal name, as
this doesn't necessarily refer to a GN executable target type; it may be
a wrapper script that invokes other binaries or scripts to run the
tests.

We expect the test executable to run one or more tests. A _test_ must be
an atomically addressable thing with a name that is unique to that
invocation of the executable, i.e., we expect that we can pass a list of
test names to the test executable and only run just those tests. Test
names must not contain a "::" (which is used as a separator between test
names) and must not contain a "*" (which could be confused with a glob
character) or start with a "-" (which would be confused with an
indicator that you should skip the test). Test names should generally
only contain ASCII code points, as the infrastructure does not currently
guarantee that non-ASCII code points will work correctly everywhere. We
do not specify test naming conventions beyond these requirements, and it
is fully permissible for a test to contain multiple assertions which may
pass or fail; this design does not specify a way to interpret or handle
those "sub-atomic" assertions; their existence is opaque to this design.
In particular, this spec does not provide a particular way to identify
and handle parameterized tests, or to do anything with test suites
beyond a supporting a limited form of globbing for specifying sets of
test names.

To configure a new test, you need to modify one to three files:

*   The test must be listed in one or more test suites in
    [//testing/buildbot/test_suites.pyl][4].  Most commonly the test will be
    defined as a single string (e.g., "base_unittests"), which keys into an
    entry in [//testing/buildbot/gn_isolate_map.pyl][5].  In some cases, tests
    will reference a target and add additional command line arguments. These
    entries (along with [//testing/buildbot/test_suite_exceptions.pyl][6] and
    [//testing/buildbot/waterfalls.pyl][7]) determine where the tests will be
    run. For more information on how these files work, see
    [//testing/buildbot/README.md][8]
*   Tests entries must ultimately reference an entry in
    //testing/buildbot/gn_isolate_map.pyl. This file contains the mapping of
    ninja compile targets to GN targets (specifying the GN label for the
    latter); we need this mapping in order to be able to run `gn analyze`
    against a patch to see which targets are affected by a patch. This file
    also tells MB what kind of test an entry is (so we can form the correct
    command line) and may specify additional command line flags. If you are
    creating a test that is only a variant of an existing test, this may be the
    only file you need to modify. (Technically, you could define a new test
    solely in test_suites.pyl and reference existing gn_isolate_map.pyl
    entries, but this is considered bad practice).
*   Add the GN target itself to the appropriate build files. Make sure this GN
    target contains all of the data and data_deps entries needed to ensure the
    test isolate has all the files the test needs to run.  If your test doesn't
    depend on new build targets or add additional data file dependencies, you
    likely don't need this. However, this is increasingly uncommon.

### Command Line Arguments

The executable must support the following command line arguments (aka flags):

```
--isolated-outdir=[PATH]
```

This argument is required, and should be set to the directory created
by the swarming task for the task to write outputs into.

```
--out-dir=[PATH]
```

This argument mirrors `--isolated-outdir`, but may appear in addition to
it depending on the bot configuration (e.g. IOS bots that specify the
`out_dir_arg` mixin in //testing/buildbot/waterfalls.pyl). It only needs
to be handled in these cases.

```
--isolated-script-test-output=[FILENAME]
```

This argument is optional. If this argument is provided, the executable
must write the results of the test run in the [JSON Test
Results Format](json_test_results_format.md) into
that file. If this argument is not given to the executable, the
executable must not write the output anywhere. The executable should
only write a valid version of the file, and generally should only do
this at the end of the test run. This means that if the run is
interrupted, you may not get the results of what did run, but that is
acceptable.

```
--isolated-script-test-filter=[STRING]
```

This argument is optional. If this argument is provided, it must be a
double-colon-separated list of strings, where each string either
uniquely identifies a full test name or is a prefix plus a "*" on the
end (to form a glob). The executable must run only the test matching
those names or globs. "*" is _only_ supported at the end, i.e., 'Foo.*'
is legal, but '*.bar' is not. If the string has a "-" at the front, the
test (or glob of tests) must be skipped, not run. This matches how test
names are specified in the simple form of the [Chromium Test List
Format][9]. We use the double
colon as a separator because most other common punctuation characters
can occur in test names (some test suites use URLs as test names, for
example). This argument may be provided multiple times; how to treat
multiple occurrences (and how this arg interacts with
--isolated-script-test-filter-file) is described below.

```
--isolated-script-test-filter-file=[FILENAME]
```

If provided, the executable must read the given filename to determine
which tests to run and what to expect their results to be. The file must
be in the [Chromium Test List Format][9] (either the simple or
tagged formats are fine). This argument may be provided multiple times;
how to treat multiple occurrences (and how this arg interacts with
`--isolated-script-test-filter`) is described below.

```
--isolated-script-test-launcher-retry-limit=N
```

By default, tests are run only once if they succeed. If they fail, we
will retry the test up to N times (so, for N+1 total invocations of the
test) looking for a success (and stop retrying once the test has
succeed). By default, the value of N is 3. To turn off retries, pass
`--isolated-script-test-launcher-retry-limit=0`. If this flag is provided,
it is an error to also pass `--isolated-script-test-repeat` (since -repeat
specifies an explicit number of times to run the test, it makes no sense
to also pass --retry-limit).

```
--isolated-script-test-repeat=N
```

If provided, the executable must run a given test N times (total),
regardless of whether the test passes or fails. By default, tests are
only run once (N=1) if the test matches an expected result or passes,
otherwise it may be retried until it succeeds, as governed by
`--isolated-script-test-launcher-retry-limit`, above. If this flag is
provided, it is an error to also pass
`--isolated-script-test-launcher-retry-limit` (since -repeat specifies an
explicit number of times to run the test, it makes no sense to also pass
-retry-limit).

```
--xcode-build-version [VERSION]
```

This flag is passed to scripts on IOS bots only, due to the `xcode_14_main`
mixin in //testing/builtbot/waterfalls.pyl.

```
--xctest
```

This flag is passed to scripts on IOS bots only, due to the `xctest`
mixin in //testing/builtbot/waterfalls.pyl.

If "`--`" is passed as an argument:

*   If the executable is a wrapper that invokes another underlying
    executable, then the wrapper must handle arguments passed before the
    "--" on the command line (and must error out if it doesn't know how
    to do that), and must pass through any arguments following the "--"
    unmodified to the underlying executable (and otherwise ignore them
    rather than erroring out if it doesn't know how to interpret them).
*   If the executable is not a wrapper, but rather invokes the tests
    directly, it should handle all of the arguments and otherwise ignore
    the "--". The executable should error out if it gets arguments it
    can't handle, but it is not required to do so.

If "--" is not passed, the executable should error out if it gets
arguments it doesn't know how to handle, but it is not required to do
so.

If the test executable produces artifacts, they should be written to the
location specified by the dirname of the `--isolated-script-test-output`
argument). If the `--isolated-script-test-output-argument` is not
specified, the executable should store the tests somewhere under the
root_build_dir, but there is no standard for how to do this currently
(most tests do not produce artifacts).

The flag names are purposely chosen to be long in order to not conflict
with other flags the executable might support.

### Environment variables

The executable must check for and honor the following environment variables:

```
GTEST_SHARD_INDEX=[N]
```

This environment variable is optional, but if it is provided, it
partially determines (along with `GTEST_TOTAL_SHARDS`) which fixed
subset of tests (or "shard") to run. `GTEST_TOTAL_SHARDS` must also be
set, and `GTEST_SHARD_INDEX` must be set to an integer between 0 and
`GTEST_TOTAL_SHARDS`. Determining which tests to run is described
below.

```
GTEST_TOTAL_SHARDS=[N]
```

This environment variable is optional, but if it is provided, it
partially determines (along with `GTEST_TOTAL_SHARDS`) which fixed subset
of tests (or "shard") to run. It must be set to a non-zero integer.
Determining which tests to run is described below.

### Exit codes (aka return codes or return values)

The executable must return 0 for a completely successful run, and a
non-zero result if something failed. The following codes are recommended
(2 and 130 coming from UNIX conventions):

| Value    | Meaning |
|--------- | ------- |
| 0 (zero) | The executable ran to completion and all tests either ran as expected or passed unexpectedly.          |
| 1        | The executable ran to completion but some tests produced unexpectedly failing results.                 |
| 2        | The executable failed to start, most likely due to unrecognized or unsupported command line arguments. |
| 130      | The executable run was aborted the user (or caller) in a semi-orderly manner (aka SIGKILL or Ctrl-C).  |

## Filtering which tests to run

By default, the executable must run every test it knows about. However,
as noted above, the `--isolated-script-test-filter` and
`--isolated-script-test-filter-file` flags can be used to customize which
tests to run. Either or both flags may be used, and either may be
specified multiple times.

The interaction is as follows:

*   A test should be run only if it would be run when **every** flag is
    evaluated individually.
*   A test should be skipped if it would be skipped if **any** flag was
    evaluated individually.

If multiple filters in a flag match a given test name, the longest match
takes priority (longest match wins). I.e.,. if you had
`--isolated-script-test-filter='a*::-ab*'`, then `ace.html` would run but
`abd.html` would not. The order of the filters should not matter. It is
an error to have multiple expressions of the same length that conflict
(e.g., `a*::-a*`).

Examples are given below.

It may not be obvious why we need to support these flags being used multiple
times, or together. There are two main sets of reasons:
*   First, you may want to use multiple -filter-file arguments to specify
    multiple sets of test expectations (e.g., the base test expectations and
    then MSAN-specific expectations), or to specify expectations in one file
    and list which tests to run in a separate file.
*   Second, the way the Chromium recipes work, in order to retry a test step to
    confirm test failures, the recipe doesn't want to have to parse the
    existing command line, it just wants to append
    --isolated-script-test-filter and list the
    tests that fail, and this can cause the --isolated-script-test-filter
    argument to be listed multiple times (or in conjunction with
    --isolated-script-test-filter-file).

You cannot practically use these mechanisms to run equally sized subsets of the
tests, so if you want to do the latter, use `GTEST_SHARD_INDEX` and
`GTEST_TOTAL_SHARDS` instead, as described in the next section.

## Running equally-sized subsets of tests (shards)

If the `GTEST_SHARD_INDEX` and `GTEST_TOTAL_SHARDS` environment variables are
set, `GTEST_TOTAL_SHARDS` must be set to a non-zero integer N, and
`GTEST_SHARD_INDEX` must be set to an integer M between 0 and N-1. Given those
two values, the executable must run only every N<sup>th</sup> test starting at
test number M (i.e., every i<sup>th</sup> test where (i mod N) == M).  dd

This mechanism produces roughly equally-sized sets of tests that will hopefully
take roughly equal times to execute, but cannot guarantee the latter property
to any degree of precision. If you need them to be as close to the same
duration as possible, you will need a more complicated process. For example,
you could run all of the tests once to determine their individual running
times, and then build up lists of tests based on that, or do something even
more complicated based on multiple test runs to smooth over variance in test
execution times. Chromium does not currently attempt to do this for functional
tests, but we do something similar for performance tests in order to better
achieve equal running times and device affinity for consistent results.

You cannot practically use the sharding mechanism to run a stable named set of
tests, so if you want to do the latter, use the `--isolated-script-test-filter`
flags instead, as described in the previous section.

Which tests are in which shard must be determined **after** tests have been
filtered out using the `--isolated-script-test-filter(-file)` flags.

The order that tests are run in is not otherwise specified, but tests are
commonly run either in lexicographic order or in a semi-fixed random order; the
latter is useful to help identify inter-test dependencies, i.e., tests that
rely on the results of previous tests having run in order to pass (such tests
are generally considered to be undesirable).

## Examples

Assume that out/Default is a debug build (i.e., that the "Debug" tag will
apply), and that you have tests named Foo.Bar.bar{1,2,3}, Foo.Bar.baz,
and Foo.Quux.quux, and the following two filter files:

```sh
$ cat filter1
Foo.Bar.*
-Foo.Bar.bar3
$ cat filter2
# tags: [ Debug Release ]
[ Debug ] Foo.Bar.bar2 [ Skip ]
$
```

#### Filtering tests on the command line

```sh
$ out/Default/bin/run_foo_tests \
    --isolated_script-test-filter='Foo.Bar.*::-Foo.Bar.bar3'
[1/2] Foo.Bar.bar1 passed in 0.1s
[2/2] Foo.Bar.bar2 passed in 0.13s

2 tests passed in 0.23s, 0 skipped, 0 failures.
$
```

#### Using a filter file

```sh
$ out/Default/bin/run_foo_tests --isolated-script-test-filter-file=filter1
[1/2] Foo.Bar.bar1 passed in 0.1s
[2/2] Foo.Bar.bar2 passed in 0.13s

2 tests passed in 0.23s, 0 skipped, 0 failures.
```

#### Combining multiple filters

```sh
$ out/Default/bin/run_foo_tests --isolated-script-test-filter='Foo.Bar.*' \
    --isolated-script-test-filter='Foo.Bar.bar2'
[1/1] Foo.Bar.bar2 passed in 0.13s

All 2 tests completed successfully in 0.13s
$ out/Default/bin/run_foo_tests --isolated-script-test-filter='Foo.Bar.* \
    --isolated-script-test-filter='Foo.Baz.baz'
No tests to run.
$ out/Default/bin/run_foo_tests --isolated-script-test-filter-file=filter2 \
    --isolated-script-test-filter=-FooBaz.baz
[1/4] Foo.Bar.bar1 passed in 0.1s
[2/4] Foo.Bar.bar3 passed in 0.13s
[3/4] Foo.Baz.baz passed in 0.05s

3 tests passed in 0.28s, 2 skipped, 0 failures.
$
```

#### Running one shard of tests

```sh
$ GTEST_TOTAL_SHARDS=3 GTEST_SHARD_INDEX=1 out/Default/bin/run_foo_tests
Foo.Bar.bar2 passed in 0.13s
Foo.Quux.quux1 passed in 0.02s

2 tests passed in 0.15s, 0 skipped, 0 failures.
$
```

## Related Work

This document only partially makes sense in isolation.

The [JSON Test Results Format](json_test_results_format.md) document
specifies how the results of the test run should be reported.

The [Chromium Test List Format][14] specifies in more detail how we can specify
which tests to run and which to skip, and whether the tests are expected to
pass or fail.

Implementing everything in this document plus the preceding three documents
should fully specify how tests are run in Chromium. And, if we do this,
implementing tools to manage tests should be significantly easier.

[On Naming Chromium Builders and Build Steps][15] is a related proposal that
has been partially implemented; it is complementary to this work, but not
required.

[Cleaning up the Chromium Testing Conventions][3] describes a series of
changes we might want to make to this API and the related infrastructure to
simplify things.

Additional documents that may be of interest:
*   [Testing Configuration Files][8]
*   [The MB (Meta-Build wrapper) User Guide][10]
*   [The MB (Meta-Build wrapper) Design Spec][11]
*   [Test Activation / Deactivation (TADA)][12] (internal Google document only,
    sorry)
*   [Standardize Artifacts for Chromium Testing][13] is somewhat dated but goes
    into slightly greater detail on how to store artifacts produced by tests
    than the JSON Test Results Format does.

## Document history

\[ Significant changes only. \]

| Date       | Comment  |
| ---------- | -------- |
| 2017-12-13 | Initial version. This tried to be a full-featured spec that defined common flags that devs might want with friendly names, as well the flags needed to run tests on the bots. |
| 2019-05-24 | Second version. The spec was significantly revised to just specify the minimal subset needed to run tests consistently on bots given the current infrastructure. |
| 2019-05-29 | All TODOs and discussion of future work was stripped out; now the spec only specifies how the `isolated_scripts` currently behave. Future work was moved to a new doc, [Cleaning up the Chromium Testing Environment][3]. |
| 2019-09-16 | Add comment about ordering of filters and longest match winning for `--isolated-script-test-filter`. |
| 2020-07-01 | Moved into the src repo and converted to Markdown. No content changes otherwise. |

## Notes

(*) The initial version of this document talked about test runners instead of
test executables, so the bit.ly shortcut URL refers to the test-runner-api instead of
the test-executable-api. The author attempted to create a test-executable-api link,
but pointed it at the wrong document by accident. bit.ly URLs can't easily be
updated :(.

[1]: https://bit.ly/chromium-test-runner-api
[2]: https://chromium.googlesource.com/infra/infra/+/main/doc/users/services/about_luci.md
[3]: https://docs.google.com/document/d/1MwnIx8kavuLSpZo3JmL9T7nkjTz1rpaJA4Vdj_9cRYw/edit?usp=sharing
[4]: ../../testing/buildbot/test_suites.pyl
[5]: ../../testing/buildbot/gn_isolate_map.pyl
[6]: ../../testing/buildbot/test_suite_exceptions.pyl
[7]: ../../testing/buildbot/waterfalls.pyl
[8]: ../../testing/buildbot/README.md
[9]: https://bit.ly/chromium-test-list-format
[10]: ../../tools/mb/docs/user_guide.md
[11]: ../../tools/mb/docs/design_spec.md
[12]: https://goto.google.com/chops-tada
[13]: https://bit.ly/chromium-test-artifacts
[14]: https://bit.ly/chromium-test-list-format
[15]: https://bit.ly/chromium-build-naming
