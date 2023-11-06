# Fuzz testing in Chromium

[go/chrome-fuzzing](https://goto.google.com/chrome-fuzzing)

[Fuzzing] is a testing technique that feeds auto-generated inputs to a piece
of target code in an attempt to crash the code. It's one of the most effective
methods we have for finding security and stability issues (see
[go/fuzzing-success](http://go/fuzzing-success)). You can learn more about the
benefits of fuzzing at [go/why-fuzz](http://go/why-fuzz).

This documentation covers the in-process guided fuzzing approach employed by
different fuzzing engines, such as [libFuzzer] or [AFL]. To learn more about
out-of-process fuzzers, please refer to the [Blackbox fuzzing] page in the
ClusterFuzz documentation.

[TOC]

## Getting Started

In Chromium, you can easily create and submit fuzz targets. The targets are
automatically discovered by buildbots, built with different fuzzing engines,
then uploaded to the distributed [ClusterFuzz] fuzzing system to run at scale.

You should fuzz any code which absorbs inputs from untrusted sources, such
as the web. If the code parses, decodes, or otherwise manipulates that input,
it's an especially good idea to fuzz it.

Create your first fuzz target and submit it by stepping through our [Getting
Started Guide].

## Advanced Topics

* [Using libfuzzer instead of FuzzTest].
* [Improving fuzz target efficiency].
* [Creating a fuzz target that expects a protobuf] instead of a byte stream as
  input.

  *** note
  **Note:** You can also fuzz code that needs multiple mutated
  inputs, or to generate inputs defined by a grammar.
  ***

* [Reproducing bugs] found by libFuzzer/AFL and reported by ClusterFuzz.
* [Fuzzing mojo interfaces] using automatically generated libprotobuf-mutator fuzzers.

## Further Reading

* [LibFuzzer integration] with Chromium and ClusterFuzz.
* [Detailed references] for other integration parts.
* Writing fuzzers for the [non-browser parts of Chrome OS].
* [Fuzzing browsertests] if you need to fuzz multiple Chrome subsystems.

## Trophies
* [Issues automatically filed] by ClusterFuzz.
* [Issues filed manually] after running fuzz targets.
* [Bugs found in PDFium] by manual fuzzing.
* [Bugs found in open-source projects] with libFuzzer.

## Other Links
* [Guided in-process fuzzing of Chrome components] blog post.
* [ClusterFuzz Stats] for fuzz targets built with AddressSanitizer and
  libFuzzer.

[Blackbox fuzzing]: https://google.github.io/clusterfuzz/setting-up-fuzzing/blackbox-fuzzing/
[Bugs found in open-source projects]: http://llvm.org/docs/LibFuzzer.html#trophies
[Bugs found in PDFium]: https://bugs.chromium.org/p/pdfium/issues/list?can=1&q=libfuzzer&colspec=ID+Type+Status+Priority+Milestone+Owner+Summary&cells=tiles
[ClusterFuzz]: https://clusterfuzz.com/
[ClusterFuzz Stats]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_chrome_asan
[Creating a fuzz target that expects a protobuf]: libprotobuf-mutator.md
[Detailed references]: reference.md
[Fuzzing]: https://en.wikipedia.org/wiki/Fuzzing
[Fuzzing browsertests]: fuzzing_browsertests.md
[Fuzzing mojo interfaces]: ../../mojo/docs/mojolpm.md
[Getting Started Guide]: getting_started.md
[Guided in-process fuzzing of Chrome components]: https://security.googleblog.com/2016/08/guided-in-process-fuzzing-of-chrome.html
[Improving fuzz target efficiency]: efficient_fuzzing.md
[Issues automatically filed]: https://bugs.chromium.org/p/chromium/issues/list?sort=-modified&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&q=label%3AStability-LibFuzzer%2CStability-AFL%20label%3AClusterFuzz%20-status%3AWontFix%2CDuplicate&can=1
[Issues filed manually]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3AStability-LibFuzzer+-label%3AClusterFuzz&sort=-modified&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids
[non-browser parts of Chrome OS]: https://chromium.googlesource.com/chromiumos/docs/+/main/testing/fuzzing.md
[Reproducing bugs]: reproducing.md
[crbug.com/539572]: https://bugs.chromium.org/p/chromium/issues/detail?id=539572
[go/fuzzing-success]: https://goto.google.com/fuzzing-success
[libFuzzer]: http://llvm.org/docs/LibFuzzer.html
[libFuzzer integration]: libFuzzer_integration.md
[Using libfuzzer instead of FuzzTest]: getting_started_with_libfuzzer.md
