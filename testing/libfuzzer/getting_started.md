# Getting started with fuzzing in Chromium

This document walks through how to get started adding fuzz tests to Chromium.

It guides you how to use our latest fuzzing technology, called [FuzzTest]. This
replaces earlier technology called [libfuzzer]. See the section at the end
for reasons why you might sometimes still want to use libfuzzer.

[TOC]

## What to fuzz

You should fuzz any function which takes input from any
untrusted source, such as the internet. If the code parses, decodes, or
otherwise manipulates that input, it definitely should be fuzzed!

To decide how best to fuzz it, you should decide which of these two situations
best matches your input:

* *Binary data*: the input is a single buffer of contiguous bytes, for example
  an image or some binary format which your code decodes.
* *Function arguments*: the input is multiple different chunks of data, for
  example the arguments to a function.

In the latter case, go ahead and read this guide - it will show you how to use
our latest fuzzing technology, FuzzTest.

If however your input more closely matches the former description - just a
single binary blob of data - then instead use our older fuzzing technology
[libfuzzer] - click that link for a separate getting started guide. (libfuzzer
will work a little better in these cases because if the fuzzer finds a problem,
the test case will exactly match the binary format.)

## How to fuzz

1. Find your existing unit test target. Create a new similar target
   alongside. (In the future, you'll be able to add them right into your
   unit test code directly.)
2. Add a gn target definition a lot like a normal unit test, but with
   `fuzztests = [ list-of-fuzztests ]`. See below for details. Create a `.cc` file.
3. In the unit tests code, `#include "third_party/fuzztest/src/fuzztest/fuzztest.h"`
4. Add a `FUZZ_TEST` macro, which might be as simple as `FUZZ_TEST(MyApiTest, ExistingFunctionWhichTakesUntrustedInput)`
   (though you may wish to structure things differently, see below)
5. Run the unit tests and ensure they pass.
6. Land the CL.

That's it!

This fuzzer will be built automatically, using various [sanitizers], and run
on our distributed fuzzing infrastructure [ClusterFuzz]. If it finds bugs,
they'll be reported back to you.

More detail in all the following sections.

## Creating a new `FUZZ_TEST` target

```
import("//testing/test.gni")

test("hypothetical_fuzztests") {
  sources = [ "hypothetical_fuzztests.cc" ]

  fuzztests = ["MyApiTest.MyApiCanSuccessfullyParseAnyString"]

  deps = [
    ":hypothetical_component",
    "//third_party/fuzztest:fuzztest_gtest_main",
  ]
}
```

You may also need to add `third_party/fuzztest` to your DEPS file.

## Adding `FUZZ_TEST` support to a target

*** note
**Note:** Currently, you must create a **new** unit test target.
While the FuzzTest framework supports mixed unit and fuzz tests,
we don't yet support this option in Chromium.
***

In the near future we'll support adding `FUZZ_TEST`s alongside existing
unit tests, even in the same .cc file.

```
test("existing_unit_tests") {
  sources = [ "existing_unit_tests.cc" ] # add FUZZ_TESTs here

  fuzztests = ["MyApiTest.ApiWorksAlways"]
    # Add this!

  deps = [
    ":existing_component",
    # Other stuff
  ]
}
```

This will:
* add a dependency on the appropriate fuzztest libraries;
* cause the target to be built on all our [fuzzer builders]
* construct metadata so that [ClusterFuzz] knows how to run the resulting
  binary.

This relies on something, somewhere, calling `base::LaunchUnitTests` within
your executable to initialize FuzzTest. This should be the case already.

(If you have other code targets, such as `source_set`s, contributing to your
unit test target they may need to explicitly depend upon `//third_party/fuzztest`
too.)

*** note
**Note:** Again, this is not yet supported!
***

## Adding `FUZZ_TEST`s in the code

First, `#include "third_party/fuzztest/src/fuzztest/fuzztest.h"`.

Then, it's normal to create a function named after the thing you're trying to
prove, with assertions to prove it.

For instance,

```
void MyApiCanSuccessfullyParseAnyString(std::string input) {
    bool success = MyApi(input);
    EXPECT_TRUE(success);
}
```

Then, declare the `FUZZ_TEST` macro:

```
FUZZ_TEST(MyApiTest, MyApiCanSuccessfullyParseAnyString);
```

Our fuzzing infrastructure will generate all possible strings and prove it works.
Obviously, that takes infinite time, so instead our fuzzing infrastructure will
carefully craft strings to explore more and more branches within `MyApi`,
mutating the input according to code coverage, so there's a good chance bugs
will be found quickly.

Fuzzing should always be alongside traditional unit testing - never rely on it
to find all the bugs! It should be a backstop to prevent unexpected security
flaws sneaking past your regular testing.

In more complex cases, you'll need to tell FuzzTest about the expected domains
of valid input. For example:

```
void MyApiAlwaysSucceedsOnPositiveIntegers(int i) {
  bool success = MyApi(i);
  EXPECT_TRUE(success);
}
FUZZ_TEST(MyApiTest, MyApiAlwaysSucceedsOnPositiveIntegers)
    .WithDomains(/*i:*/fuzztest::Positive<int>());
```

See the [FuzzTest reference] for all your options here.

## Running this locally

Simply build and run your unit tests as normal. `FUZZ_TEST`s are supported only
on some platforms. If you're on such a platform, you'll see your fuzz test
run for one second:

```
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from ScaleFuzz
[ RUN      ] ApiTest.MyApiCanSuccessfullyParseAnyString
[       OK ] ApiTest.MyApiCanSuccessfullyParseAnyString (1000 ms)
[----------] 1 test from ScaleFuzz (1000 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (1000 ms total)
[  PASSED  ] 1 test.
```

On other platforms, the test will be ignored.

If you want to try actually fuzzing with FuzzTest, modify your gn arguments to
contain:

```
enable_fuzztest_fuzz=true
is_component_build=false
```

You can then run your unit test with the extra command line argument `--fuzz=`,
optionally specifying a test name. You'll see lots of output as it explores your
code:

```
[*] Corpus size:     1 | Edges covered:     73 | Fuzzing time:        1.60482ms | Total runs:  1.00e+00 | Runs/secs:   623 | Max stack usage:        0
[*] Corpus size:     2 | Edges covered:    103 | Fuzzing time:          1.844ms | Total runs:  2.00e+00 | Runs/secs:  1084 | Max stack usage:        0
[*] Corpus size:     3 | Edges covered:    111 | Fuzzing time:       2.747931ms | Total runs:  3.00e+00 | Runs/secs:  1091 | Max stack usage:        0
[*] Corpus size:     4 | Edges covered:    135 | Fuzzing time:        2.92305ms | Total runs:  4.00e+00 | Runs/secs:  1368 | Max stack usage:        0
[*] Corpus size:     5 | Edges covered:    173 | Fuzzing time:        3.35237ms | Total runs:  5.00e+00 | Runs/secs:  1491 | Max stack usage:        0
[*] Corpus size:     6 | Edges covered:    178 | Fuzzing time:        4.15666ms | Total runs:  6.00e+00 | Runs/secs:  1443 | Max stack usage:        0
```

("Edges covered") is how many different code blocks have been explored (that is,
sections between branches). Over time, you'll see it explore more and more until
it runs out of new edges to explore.

## Landing the CL

Nothing special is required here!

After a day or two, we should see [ClusterFuzz] starting to run your new fuzzer,
and it should be visible on [ClusterFuzz Fuzzer Stats]. Look for fuzzers starting
with `centipede_` and your test target's name.

*** note
**Note:** This is all very new, and ClusterFuzz isn't reliably spotting these
new fuzztests yet. We're working on it!
***

Thanks very much for doing your part in making Chromium more secure!

## Unusual cases

There are some situations where FuzzTests may not work. For example:

* You need to run on platforms not currently supported by FuzzTest
* You need more structured input
* You need to mutate the input in a more precise way
* Your fuzzer input is a single binary blob

In these cases, you may be best off creating a standalone fuzzer using our
older fuzzing technology, [libfuzzer]. There are further options beyond
that, e.g. uploading "black box" fuzzers to ClusterFuzz, or even running
fuzzers outside of ClusterFuzz which then upload results to ClusterFuzz
for triage and diagnosis. To explore any of those options, please discuss
with the fuzzing team (email security@chromium.org if you're outside Google).


[FuzzTest]: https://github.com/google/fuzztest#how-do-i-use-it
[libfuzzer]: getting_started_with_libfuzzer.md
[`test` template]: https://source.chromium.org/chromium/chromium/src/+/main:testing/test.gni?q=test.gni
[fuzzer builders]: https://ci.chromium.org/p/chromium/g/chromium.fuzz/console
[ClusterFuzz]: https://clusterfuzz.com/
[FuzzTest reference]: https://github.com/google/fuzztest#how-do-i-use-it
[ClusterFuzz Fuzzer Stats]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_chrome_asan
