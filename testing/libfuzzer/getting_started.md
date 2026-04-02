# Getting started with fuzzing in Chromium

This document explains how to add fuzz tests to Chromium using [FuzzTest].

If you need to maintain an existing libFuzzer target, see
[Getting Started with libFuzzer]. libFuzzer is deprecated for new fuzz targets.

[TOC]

## What to fuzz

You should fuzz any function that takes input from an untrusted source, such as
the internet. If the code parses, decodes, or otherwise manipulates that input,
you should write a fuzz test for it.

To view directories that currently lack fuzz coverage, see
[go/chrome-fuzzing-dashboard].

## Write the FuzzTest code

We recommend writing your fuzz tests in the same file as your standard unit tests.
Open your existing unit test file and include the FuzzTest header:

```c++
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"`
```

Create a function with assertions that verify the expected behavior. For example:

```c++
void MyApiCanSuccessfullyParseAnyString(std::string input) {
    bool success = MyApi(input);
    EXPECT_TRUE(success);
}
```

In the same file, declare the `FUZZ_TEST` macro directly below your function:

```c++
FUZZ_TEST(MyApiTest, MyApiCanSuccessfullyParseAnyString);
```

The fuzzing infrastructure mutates the input based on code coverage to explore
branches within your API. Because generating all possible strings requires
infinite time, the fuzzer crafts inputs designed to maximize code coverage and
find bugs efficiently.

Fuzz tests do not replace traditional unit tests. Use fuzzing as a supplementary
testing strategy to help prevent unexpected security flaws.

For complex cases, specify the expected domains of valid input. For example:

```c++
void MyApiAlwaysSucceedsOnPositiveIntegers(int i) {
  bool success = MyApi(i);
  EXPECT_TRUE(success);
}
FUZZ_TEST(MyApiTest, MyApiAlwaysSucceedsOnPositiveIntegers)
    .WithDomains(/*i:*/fuzztest::Positive<int>());
```

For more information about configuring input domains, see the
[FuzzTest Domain Reference].

## Configure the GN build target

Now that your test is written, add it to your existing unit test target. Open
your `BUILD.gn` file, locate your existing `test` target, and add the
`fuzztests` list containing the name of your new test.

```gn
import("//testing/test.gni")

test("my_component_unittests") {
  sources = [
    "my_component_unittest.cc", # Your FUZZ_TEST macros are in this file.
  ]

  # Add your FuzzTest names here:
  fuzztests = [ "MyApiTest.MyApiCanSuccessfullyParseAnyString" ]

  deps = [
    ":my_component",

    # Chromium unit tests usually already include this dependency.
    # It provides the standard test main() that initializes FuzzTest.
    "//base/test:test_support",
  ]
}
```

Because Chromium's [`test` template] automatically injects FuzzTest dependencies
and initializes the framework, you do not need to add any specific FuzzTest
libraries to your `deps`.

## Update your DEPS file

If your component's directory enforces include rules, you must add
`+third_party/fuzztest` to your local DEPS file so that your C++ files can
successfully include the FuzzTest headers.

## Run the FuzzTest locally

In a local terminal, build and run your unit tests. If your platform supports
FuzzTest, the test runs for one second and produces the following output:

```shell
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

On unsupported platforms, the system ignores the test.

To perform continuous fuzzing with FuzzTest locally, add the following to your
gn arguments:

```gn
enable_fuzztest_fuzz=true
is_component_build=false
```

Run your unit test and append the `--fuzz=` argument. You can optionally specify
a target test name by using `--fuzz=TEST_NAME`. The command produces
the following output as it explores your code:

```shell
[*] Corpus size:     1 | Edges covered:     73 | Fuzzing time:        1.60482ms | Total runs:  1.00e+00 | Runs/secs:   623 | Max stack usage:        0
[*] Corpus size:     2 | Edges covered:    103 | Fuzzing time:          1.844ms | Total runs:  2.00e+00 | Runs/secs:  1084 | Max stack usage:        0
[*] Corpus size:     3 | Edges covered:    111 | Fuzzing time:       2.747931ms | Total runs:  3.00e+00 | Runs/secs:  1091 | Max stack usage:        0
[*] Corpus size:     4 | Edges covered:    135 | Fuzzing time:        2.92305ms | Total runs:  4.00e+00 | Runs/secs:  1368 | Max stack usage:        0
[*] Corpus size:     5 | Edges covered:    173 | Fuzzing time:        3.35237ms | Total runs:  5.00e+00 | Runs/secs:  1491 | Max stack usage:        0
[*] Corpus size:     6 | Edges covered:    178 | Fuzzing time:        4.15666ms | Total runs:  6.00e+00 | Runs/secs:  1443 | Max stack usage:        0
```

The **Edges covered** metric indicates the number of unique code blocks the
fuzzer explores. Over time, the fuzzer explores more blocks until it exhausts
new edges.

## Submit the Fuzztest

Submit your changelist by using standard Chromium contribution processes.

[ClusterFuzz] typically begins running your new fuzzer within two days. To view
the fuzzer, go to [ClusterFuzz Fuzzer Stats] and search for your test target's
name. FuzzTest targets typically use the `centipede_` or `libfuzzer_` prefix in
the dashboard.

Thanks very much for doing your part in making Chromium more secure!

## Get help with advanced cases

If FuzzTest and libFuzzer do not meet your requirements, you can explore
advanced options, such as uploading blackbox fuzzers to ClusterFuzz or running
external fuzzers that upload results to ClusterFuzz for triage. To discuss these
options, contact the fuzzing team at chrome-fuzzing-core@google.com.

[ClusterFuzz]: https://clusterfuzz.com/
[FuzzTest Domain Reference]: https://github.com/google/fuzztest/blob/main/doc/domains-reference.md
[ClusterFuzz Fuzzer Stats]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_chrome_asan
[fuzzer builders]: https://ci.chromium.org/p/chromium/g/chromium.fuzz/console
[FuzzTest]: https://github.com/google/fuzztest
[Getting Started with libFuzzer]: getting_started_with_libfuzzer.md
[go/chrome-fuzzing-dashboard]: (https://analysis.chromium.org/coverage/p/chromium?platform=fuzz).
[`test` template]: https://source.chromium.org/chromium/chromium/src/+/main:testing/test.gni?q=test.gni
