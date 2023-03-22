# GPU Expectation Files

This file goes over details of the expectation files which are critical for
ensuring that GPU tests only run where they should and that flakes are
suppressed to avoid red bots.

[TOC]

## Overview

The GPU Telemetry-based integration tests (tests that use the
`telemetry_gpu_integration_test` target)
[utilize expectation files](gpu_expectations) in order to define when certain
tests should not be run or are expected to fail. The core expectation format is
defined by [typ](typ_expectations), although there are some Chromium-specific
extensions as well. Each expectation consists of the following fields, separated
by a space:

1. An optional bug identifier. While optional, it is heavily encouraged that GPU
   expectations have this field filled.
1. A set of tags that the expectation applies to. This is technically optional,
   as omitting tags will cause the expectation to be applied everywhere, but
   there are very few, if any, instances where tags will not be specified for
   GPU expectations.
1. The name of the test that the expectation applies to. A single wildcard (`*`)
   character is allowed at the end of the string, but use of a wildcard anywhere
   but the end of the string is an error.
1. A set of expected results for the test. This technically supports multiple
   values, but for GPU purposes, it will always be a single value.

Additionally, comments are supported, which begin with `#`.

Thus, a sample expectation entry might look like:

```
# Flakes regularly but infrequently.
crbug.com/1234 [ win amd ] foo/test [ RetryOnFailure ]
```

[gpu_expectations]: https://chromium.googlesource.com/chromium/src/+/main/content/test/gpu/gpu_tests/test_expectations
[typ_expectations]: https://chromium.googlesource.com/catapult.git/+/main/third_party/typ/typ/expectations_parser.py

## Core Format

The following are further details on each of the parts of an expectation that
are part of the core expectation file format.

### Bug Identifier

An optional string(s) pointing to the bug(s) tracking the reason why the
expectation exists. For GPU uses, this is usually a single bug, but multiple
space-separated strings are supported.

The format of the string is enforced by [these](bug_regexes) regular
expressions, so CLs that introduce malformed bugs will not be submittable.

[bug_regexes]: https://chromium.googlesource.com/chromium/src/+/e26d89a52627f8910b79a95668dfa48e5fe8fa06/content/test/gpu/gpu_tests/test_expectations_unittest.py#66

### Tags

One or more tags are used to specify which configuration(s) an expectation
applies to. For GPU tests, this is often things such as the OS, the GPU vendor,
or the specific GPU model.

Tag sets are defined at the top of the expectation file using `# tags:`
comments. Each comment defines a different set of mutually exclusive tags, e.g.
all of the OS tags are in a single set. An expectation is only allowed to use
one tag from each set, but can use tags from an arbitrary number of sets. For
example, `[ win win10 ]` would be invalid since both are OS tags, but
`[ win amd release ]` would be valid since there is one tag each from the OS,
GPU, and browser type tag sets.

Additionally, tags used for expectations with the same test must be unambiguous
so that the same test cannot have multiple expectations applied to it at once.
Take the following expectations as an example:

```
[ mac intel ] foo/test [ Failure ]
[ mac debug ] foo/test [ RetryOnFailure ]
```

These expectations would be considered to be conflicting since `[ mac intel ]`
does not make any distinctions about the browser type, and `[ mac debug ]` does
not make any distinctions about the GPU type. As written, `foo/test` running
on a configuration that produced the `mac`, `intel`, and `debug` tags would try
to use both expectations.

This can be fixed by adding a tag from the same tag set but with a different
value so that the configurations are no longer ambiguous.
`[ mac intel release ]` would work since a configuration cannot be both
`release` and `debug` at the same time. Similarly, `[ mac amd debug ]` would
work since a configuration cannot be both `intel` and `amd` at the same time.

Such conflicts will be caught and reported by presubmit tests, so you should not
have to worry about accidentally landing bad expectations, but you will need to
fix any found conflicts before you can submit your CL.

#### Adding/Modifying Tags

Actually updating the test harness to generate new tags is out of scope for this
documentation. However, if a new tag needs to be added to an expectation file
or an existing one modified (e.g. renamed), it is important to note that the
tag header should not be manually modified in the expectation file itself.

Instead, modify the header in [validate_tag_consistency.py] and run
`validate_tag_consistency.py apply` to apply the new header to all expectation
files. This ensures that all files remain in sync.

Tag consistency is checked as part of presubmit, so it will be apparent if you
accidentally modify the tag header in a file directly.

[validate_tag_consistency.py]: https://chromium.googlesource.com/chromium/src/+/main/content/test/gpu/validate_tag_consistency.py

### Test Name

A single string with either a test name or part of a test name suffixed with a
wildcard character. Note that the test name is just the test case as reported
by the test harness, not the fully qualified name that is sometimes reported in
places such as the "Test Results" tab on bots.

As an example,
`gpu_tests.webgl1_conformance_integration_test.WebGL1ConformanceIntegrationTest.WebglExtension_EXT_blend_minmax`
is a fully qualified name, while `WebglExtension_EXT_blend_minmax` is what would
actually be used in the expectation file for the `webgl1_conformance` suite.

### Expected Results

Usually one, but potentially multiple, results that are expected on the
configuration that the expectation is for. Like tags, expected results are
defined at the top of each expectation file and have the same caveat about
addition/modification with the helper script. However, unlike tags, there is
only one set of values which are not expected to be added to/changed on any
sort of regular basis. The following expected results are used by GPU tests:

#### Skip

Skips the test entirely. The benefit of this is that no time is wasted on a bad
test. However, it also means that it is impossible to check if the test is still
failing or not by just looking at historical results. This is problematic for
humans, but even more problematic for scripts we have to automatically remove
expectations that are no longer needed.

As such, it is heavily discouraged to add new Skip expectations except under the
following circumstances:

1. The test is invalid on a configuration for some reason, e.g. a feature is not
   and will not be supported on a certain OS, and so should never be run. These
   sorts of expectations are expected to be permanent.
1. The act of running the test is significantly detrimental to other tests, e.g.
   running the test kills the test device. These are expected to be temporary,
   so the root cause should be fixed relatively quickly.

If presubmit thinks you are adding new Skip expectations, it will warn you, but
the warning can be ignored if the addition falls into one of the above
categories or it is a false positive, such as due to modifying tags on an
existing expectation.

#### Failure

Lets the test run normally, but hides the fact that it failed during result
reporting. This is the preferred way to suppress frequent failures on bots, as
it keeps the bots green while still reporting results that can be used later.

#### RetryOnFailure

Allows the test to be retried up to two additional times before being marked as
failing, as by default GPU tests do not retry on failure. This is preferred if
the test fails occasionally, but not enough to warrant marking it as failing
consistently.

#### Slow

Only has an effect in a subset of test suites. Currently, those are suites that
use a heartbeat mechanism instead of a fixed timeout:

* `webgpu_cts`
* `webgl1_conformance`
* `webgl2_conformance`

Since these tests use a relatively short timeout that gets refreshed as long as
the test does not hang, they are more susceptible to timeouts if the test does a
lot of work or other parallel tests are using a large number of resources. In
these cases, the `Slow` expectation can be used to increase the heartbeat
timeout for a test, reducing the chance that one of these timeouts is hit.

If the reported failure for a test is along the lines of "Timed out waiting for
websocket message", prefer to use a `Slow` expectation first over a `Failure` or
`RetryOnFailure` one.

## Extensions

In addition to the normal expectation functionality, Chromium has several
extensions to the expectation file format.

### Unexpected Pass Finder Annotations

Chromium has several unexpected pass finder scripts (sometimes called stale
expectation removers) to automatically reclaim test coverage by modifying
expectation files. These mostly work as intended, but can occasionally make
changes that don't align with what we actually want. Thus, there are several
annotations that can be inserted into expectation files to adjust the behavior
of these scripts.

#### Disable

There are several annotations that can be used to prevent the scripts from
automatically removing expectations. All of these start with `finder:disable`
with some suffix.

`finder:disable-general` prevents the expectation from being removed under any
circumstances.

`finder:disable-stale` prevents the expectation from being removed if it is
still applicable to at least one bot, but all queried results point to the
expectation no longer being needed. This is most likely to be used for
expectations for very infrequent flakes, where the flake might not occur within
the data range that we query.

`finder:disable-unused` prevents the expectation from being removed if it is
found to not be used on any bots, i.e. the specified configuration does not
appear to actually be tested. This is most likely to be used for expectations
for failures reported by third parties with their own testing configurations.

`finder:disable-narrowing` prevents the expectation from having its scope
automatically narrowed to only apply to configurations that are found to need
it. This is most likely to be used for expectations that are intentionally
broad to prevent failures that aren't planned on being fixed.

All of these annotations can either be used inline for a single expectation:

```
[ mac intel ] foo/test [ Failure ]  # finder:disable-general
```

or with their `finder:enable` equivalent for blocks:

```
# finder:disable-general
[ mac intel ] foo/test [ Failure ]
[ mac intel ] bart/test [ Failure ]
# finder:enable-general
```

Nested blocks are not allowed. The `finder:disable` annotations can be followed
with a description of why the disable is necessary, which will be output by the
script when it encounters a case where one of the disabled expectations would
have been removed if the annotation was not present:

```
# finder:disable-stale Very low flake rate
[ mac intel ] foo/test [ Failure ]
[ mac intel ] bar/test [ Failure ]
# finder:enable-stale
```

#### Group Start/End

There may be cases where groups of expectations should only be removed together,
e.g. if a flake affects a large number of tests but the chance of any individual
test hitting the flake is low. In these cases, the expectations can be grouped
together so one is only removed if all of them are being removed.

```
# finder:group-start Some group description or name
[ mac intel ] foo/test [ Failure ]
[ mac intel ] bar/test [ Failure ]
# finder:group-end
```

The group name/description is required and is used to uniquely identify each
group. This means that groups with the same name string in different parts of
the file will be treated as the same group, as if they were all in a single
group block together.

```
# finder:group-start group_name
[ mac ] foo/test [ Failure ]
[ mac ] bar/test [ Failure ]
# finder:group-end

...

# finder:group-start group_name
[ android ] foo/test [ Failure ]
[ android ] bar/test [ Failure ]
# finder:group-end
```

is equivalent to

```
# finder:group-start group_name
[ mac ] foo/test [ Failure ]
[ mac ] bar/test [ Failure ]
[ android ] foo/test [ Failure ]
[ android ] bar/test [ Failure ]
# finder:group-end
```
