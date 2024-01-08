# Expectation Files

A number of test suites in Chromium use expectation files to handle test
failures in order to have more granular control compared to the usual approach
of entirely disabling failing tests. This documentation goes into the general
usage of expecation files, while suite-specific details are handled in other
files.

[TOC]

Currently, the test suites that use expectation files can be broadly categorized
as Blink tests and GPU tests. Blink-specific documentation can be found
[here][blink_expectation_doc], while GPU-specific documentation can be found
[here][gpu_expectation_doc].

[blink_expectation_doc]: https://source.chromium.org/chromium/chromium/src/+/main:docs/testing/web_test_expectations.md
[gpu_expectation_doc]: https://source.chromium.org/chromium/chromium/src/+/main:docs/gpu/gpu_expectation_files.md

## Design

The full design for the format can be found [here][chromium_test_list_format] if
the overview in this documentation is not sufficient.

[chromium_test_list_format]: http://bit.ly/chromium-test-list-format

## Code

The parser implementation used by Chromium can be found [here][typ_parser]. This
handles the parsing of the text files into Python objects usable by Chromium's
test harnesses.

[typ_parser]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/third_party/typ/typ/expectations_parser.py

## Syntax

An expectation file can be broadly broken up into two sections: the header and
test expectations.

### Header

The header consists of specially formatted comments that define what tags and
expected results are usable in expectations later in the file. All header
content must be before any expectation content. Failure to do so will result in
the parser raising errors. An example header is:

```
# tags: [ linux ubuntu jammy
#         mac mac10 mac11 mac12 mac13
#         win win7 win10 ]
# tags: [ release debug ]
# results: [ Failure Skip Slow ]
````

Specifically, the header consists of one or more tag sets and exactly one
expected result set.

#### Tag Sets

Each tag set begins with a `# tags:` comment followed by a space-separated list
of tags between `[ ]`. Order does not matter to the parser, and tags are
case-insensitive. Tag sets can span multiple lines as long as each line starts
with `#` and all tags are within the brackets.

Each tag set contains all the tags that can be used in expectations for a
particular aspect of a test configuration. In the example header, the first tag
set contains values for operating systems, while the second tag set contains
values for browser build type. Grouping tags together into different sets
instead of having a monolithic set with all possible tag values is necessary
in order to handle conflicting expectation detection (explained later in
[the conflict section](#Conflicts)).

One important note about tag sets is that unless a test harness is implementing
custom conflict detection logic, all tags within a set should be mutually
exclusive, i.e. only one tag from each tag set should be produced when running a
test. Failure to do so can result in conflict detection false negatives, the
specifics of which are explained in [the conflict section](#Conflicts).

#### Expected Result Set

The expected result set begins with a `# results:` comment followed by a
space-separated list of expected results between `[ ]`. Order does not matter to
the parser, but expected results are case sensitive. Additionally, only values
[known to the parser][typ_known_results] can be used. The expected results can
span multiple lines as long as each line starts with `#` and all values are
within the brackets.

The expected result set contains all the expected results that can be used in
expectations. The specifics of how each expected result affects test behavior
can differ slightly between test suites, but generally do the following:

* Pass - The default expected result for all tests. Let the test run, and expect
  it to run without issue.
* Failure - Let the test run, but treat failures as a pass.
* Crash - Let the test run, but treat test failures due to crashes as a pass.
* Timeout - Let the test run, but treat test failures due to timeouts as a pass.
* Skip - Do not run the test.
* RetryOnFailure - Re-enable automatic retries of a test if a suite has them
  disabled by default.
* Slow - Indicate that the test is expected to take longer than normal, usually
  as a signal to increase timeouts.

[typ_known_results]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/third_party/typ/typ/expectations_parser.py;l=40

### Expectations

After the header, the rest of the file consists of test expectations which
specify what non-standard test behavior is expected on specific test machine
configurations. An expectation is a single line in the following format:

```
bug_identifier [ tags ] test_name [ expected_results ]
```

As an example, the following would be an expectation specifying that the
`foo.html` test is expected to fail on Windows machines with Debug browsers:

```
crbug.com/1234 [ win debug ] foo.html [ Failure ]
```

The bug identifier and tags are both optional and can be omitted. Not specifying
any tags means that the expectation applies to the test regardless of where it
is run. When omitting tags, the brackets are also omitted. Additionally,
multiple bug identifiers are allowed as long as they are space-separated. The
parser looks for certain prefixes, e.g. `crbug.com/` to determine what is
considered a bug. This allows the parser to properly disambiguate one or more
bug identifiers from the test name in the event that an expectation does not
have any tags.

Multiple expected results are allowed and are space-separated like tags. As an
example, `[ Failure Crash ]` would specify that the test is expected to either
fail or crash.

Additionally, the test name is allowed to have up to one wildcard at the very
end to match any tests that begin with the specified name. As an example, the
following would be an expectation specifying that any test starting with `foo`
is expected to fail on Windows machines with Debug browsers.

```
crbug.com/1234 [ win debug ] foo* [ Failure ]
```

#### Priority

When using wildcards, it is possible for multiple expectations to apply to a
test at runtime. For example, given the following:

```
[ win ] foo* [ Slow ]
[ win ] foo/bar* [ Failure ]
[ win ] foo/bar/specific_test.html [ Skip ]
```

`foo/bar/specific_test.html` running on a Windows machine would have three
applicable expectations. In these cases, the most specific (i.e. the
longest-named) expectation will be used.

The order in which expectations are defined is *not* considered when determining
priority.

## Conflicts

When more than one expectation exists for a test, it is possible that there will
be a conflict where a test run on a particular test machine could have more than
one expectation apply to it. Whether these conflicts are treated as errors and
how conflicts get resolved are both configurable options via annotations found
under [the annotations section](#Annotations).

### Detection

Two expectations for the same test conflict with each other if they do not use
different tags from at least one shared tag set. As an example, look at the
following expectations:

```
# Group 1
[ win ] foo.html [ Failure ]
[ mac ] foo.html [ Skip ]

# Group 2
[ win ] bar.html [ Failure ]
[ debug ] bar.html [ Skip ]

# Group 3
[ linux ] foo.html [ Failure ]
[ linux debug ] foo.html [ Skip ]
```

Group 1 would not result in a conflict since both `win` and `mac` are from the
same tag set and are different values. Thus, the parser would be able to
determine that at most one expectation will apply when running a test.

Group 2 would result in a conflict since there are no tag sets that both
expectations use, and thus there could be a test configuration that causes both
expectations to apply. In this case, a configuration that produces both the
`win` and `debug` tags is possible. This conflict could be resolved by adding
a browser type tag to the first expectation or an operating system tag to the
second expectation.

Group 3 would result in a conflict since there is a tag set that both
expectations use (operating system), but the exact tag is the same. Thus, a
test running on Linux with a Debug browser would have both expectations apply.
This conflict could be resolved by changing the first expectation to use
`[ linux release ]`.

It is important to be aware of the following when it comes to conflicts:

1. The expectation file has no knowledge of which tag combinations are actually
   possible in the real world, only what is theoretically possible given the
   defined tag sets. A real world example of this would be the use of the Metal
   API, which is Mac-specific. While a human would be able to reason that
   `[ metal ]` implies `[ mac metal ]`, the latter is necessary for the
   conflict detection to work properly.
2. If tag sets include non-mutually-exclusive values and the test suite has not
   implemented custom conflict checking logic, there can be false negatives when
   checking for conflicts. For example, if `win` and `win10` were both in the OS
   tag set, `[ win ] foo.html [ Failure ]` and `[ win10 ] foo.html [ Skip ]`
   would not be found to conflict even though they can in the real world due to
   `win10` being a more specific version of `win`.
3. Expectations that use wildcards can result in conflict detection false
   negatives. Conflict detection is only run on expectations with identical test
   names. Thus, while `[ win ] foo* [ Failure ]` and `[ debug ] foo* [ Skip ]`
   would be found to conflict since the test name is `foo*` in both cases,
   `[ win ] f* [ Failure ]` and `[ debug ] foo* [ Skip ]` would not be found to
   conflict.

### Annotations

By default, conflicts result in a parsing error. However, expectation files
support several annotations to affect how conflicts are handled.

`# conflicts_allowed: true` causes conflicts to no longer cause parsing errors.
Instead, conflicts will be handled gracefully depending on the conflict
resolution setting, the default of which is to take the union of expected
results.

`# conflict_resolution: ` specifies how conflicts will be handled when they are
allowed. Supported values are `union` (the default) and `override`. `union`
causes all conflicted expectations to be merged together. For example, the
following:

```
[ win ] foo.html [ Failure ]
[ debug ] foo.html [ Slow ]
```

would be equivalent to `[ win debug ] foo.html [ Failure Slow ]` when running on
a Windows machine with a Debug browser.

`override` uses whatever expectation was parsed last. Using the above example,
A Windows machine with a Debug browser would end up using the
`[ debug ] foo.html [ Slow ]` expectation.
