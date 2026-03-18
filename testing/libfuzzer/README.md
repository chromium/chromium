# Fuzzing in Chromium

[go/chrome-fuzzing](https://goto.google.com/chrome-fuzzing)

  *** note
  **Just got a bug report from ClusterFuzz?:** If you want to reproduce a
  ClusterFuzz crash locally, see [How to Reproduce a Crash from ClusterFuzz].
  ***

[Fuzzing] is an automated software testing technique that provides invalid,
unexpected, or random data as inputs to a program to find bugs.

**Why fuzz?** Fuzzing finds thousands of security and stability issues before
they reach users (see [go/fuzzing-success]). For more information about the
benefits of fuzzing, see [go/why-fuzz].

**Where to fuzz?** Fuzz code that parses, decodes, or manipulates input from
untrusted sources, such as the web.

[TOC]

## Getting started

In Chromium, you can create and submit fuzz targets that run continuously at
scale on [ClusterFuzz]. Prefer FuzzTest for all new fuzz targets. Use libFuzzer
only to maintain existing targets.

### FuzzTest (recommended)

FuzzTest integrates with the gtest framework and tests code that accepts
structured, typed inputs, such as `int`, `std::string`, `std::vector`, or custom
classes.

*   To write your first FuzzTest, see [Getting Started with FuzzTest].
*   To visualize the impact of your fuzzer, see
    [Generating local code coverage reports for FuzzTests].

### libFuzzer (deprecated)

[libFuzzer] tests APIs that consume raw byte buffers, such as image decoders and
JSON or XML parsers.

*   To write, build, and run a basic libFuzzer target, see
    [Getting Started with libFuzzer].
*   To make your fuzzer more effective using seed corpuses and dictionaries,
    see [Improving fuzz target efficiency].

## Advanced topics

*   [Getting Started with libprotobuf-mutator (LPM)] - Fuzz code that expects a
    protobuf, has multiple inputs, or is defined by a grammar.

*   [Fuzzing mojo interfaces] - A guide for using LPM to fuzz Mojo interfaces.

*   [Fuzzing in Chrome OS] - Writing fuzzers for the non-browser parts of Chrome
    OS.

*   [Fuzzing browsertests] - For fuzzing multiple Chrome subsystems that require
    a full browser environment.

*   [libFuzzer Integration Details] - The specifics of how libFuzzer integrates
    with Chromium and ClusterFuzz.

*   [libfuzzer Technical References] - A detailed reference for build arguments
    (GN), sanitizer configurations, platform support, and ClusterFuzz options.

*   [Blackbox fuzzing] - Fuzz large, slow, or non-deterministic targets without
    coverage guidance.

## Getting help

If you have questions or encounter issues,

*   email `chrome-fuzzing-core@google.com` or
*   file a bug using the **Chrome > Security > Fuzzing** component.

## View dashboard and stats

*   [go/chrome-fuzzing-dashboard] - View the code coverage achieved by fuzzers
    in Chromium.
*   [ClusterFuzz Stats] - Performance statistics for fuzzers.

[Blackbox fuzzing]: https://google.github.io/clusterfuzz/reference/coverage-guided-vs-blackbox/#blackbox-fuzzing
[ClusterFuzz]: https://clusterfuzz.com/
[ClusterFuzz Stats]: https://clusterfuzz.com/fuzzer-stats/
[Fuzzing]: https://en.wikipedia.org/wiki/Fuzzing
[Fuzzing browsertests]: fuzzing_browsertests.md
[Fuzzing in Chrome OS]: https://www.chromium.org/chromium-os/developer-library/guides/testing/fuzzing/
[Fuzzing mojo interfaces]: ../../mojo/docs/mojolpm.md
[Generating local code coverage reports for FuzzTests]: fuzz_test_coverage.md
[Getting Started with FuzzTest]: getting_started.md
[Getting Started with libfuzzer]: getting_started_with_libfuzzer.md
[Getting Started with libprotobuf-mutator (LPM)]: libprotobuf-mutator.md
[go/chrome-fuzzing-dashboard]: https://analysis.chromium.org/coverage/p/chromium?platform=fuzz&test_suite_type=any&path=%2F%2F&project=chromium%2Fsrc&path=%2F%2F&host=chromium.googlesource.com&ref=refs%2Fheads%2Fmain&modifier_id=0
[go/fuzzing-success]: http://go/fuzzing-success
[go/why-fuzz]: http://go/why-fuzz
[How to Reproduce a Crash from ClusterFuzz]: reproducing.md
[Improving fuzz target efficiency]: efficient_fuzzing.md
[libFuzzer]: http://llvm.org/docs/LibFuzzer.html
[libFuzzer Integration Details]: libFuzzer_integration.md
[libFuzzer technical references]: reference.md
