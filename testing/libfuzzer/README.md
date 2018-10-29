# libFuzzer in Chromium

[go/libfuzzer-chromium](https://goto.google.com/libfuzzer-chromium) (Googler only)

*** aside
[Getting Started](getting_started.md)
| [Buildbot](https://build.chromium.org/p/chromium.fyi/buildslaves/slave43-c1)
| [ClusterFuzz Status](https://clusterfuzz.com/v2/fuzzer-stats)
| [Cover Bug]
***

This directory contains integration between [libFuzzer] and Chromium.
LibFuzzer is an in-process coverage-driven evolutionary fuzzing engine. It helps
engineers to uncover potential security & stability problems.

*** note
**Requirements:** libFuzzer in Chromium is supported with Linux, Mac, and
Windows only.
***

## Integration Status

Fuzzer tests are well-integrated with Chromium build system and distributed
ClusterFuzz fuzzing system. Cover bug: [crbug.com/539572].

## Documentation

* [Getting Started Guide] walks you through all the steps necessary to create
your fuzz target and submit it to ClusterFuzz.
* [Efficient Fuzzer Guide] explains how to measure fuzz target effectiveness and
ways to improve it.
* [Guide to libprotobuf-mutator] walks through the steps necessary to create a
fuzz target that expects a protobuf as input (instead of a byte stream). In
addition to fuzzing code that accepts protobufs, it can be used to fuzz
code that requires multiple mutated inputs, or to generate inputs defined by a
grammar.
* [ClusterFuzz Integration] describes integration between ClusterFuzz and
libFuzzer.
* [Reproducing] contains information on how to reproduce bugs reported by
  ClusterFuzz.
* [Reference] contains detailed references for different integration parts.

## Trophies
* [ClusterFuzz Bugs] - issues found and automatically filed by ClusterFuzz.
* [Manual Bugs] - issues that were filed manually after running fuzz targets.
* [Pdfium Bugs] - bugs found in pdfium by manual fuzzing.
* [OSS Trophies] - bugs found with libFuzzer in open-source projects.


## Blog Posts
* [Guided in-process fuzzing of Chrome components].

## Project Links
* [libFuzzer Infrastructure Bugs]

[libFuzzer]: http://llvm.org/docs/LibFuzzer.html
[crbug.com/539572]: https://bugs.chromium.org/p/chromium/issues/detail?id=539572
[Cover Bug]: https://bugs.chromium.org/p/chromium/issues/detail?id=539572
[Getting Started Guide]: getting_started.md
[Efficient Fuzzer Guide]: efficient_fuzzer.md
[Guide to libprotobuf-mutator]: libprotobuf-mutator.md
[ClusterFuzz Integration]: clusterfuzz.md
[Reproducing]: https://github.com/google/clusterfuzz-tools
[Reference]: reference.md
[ClusterFuzz Bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label:Stability-LibFuzzer%20label:ClusterFuzz&sort=-modified&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified
[Pdfium Bugs]: https://bugs.chromium.org/p/pdfium/issues/list?can=1&q=libfuzzer&colspec=ID+Type+Status+Priority+Milestone+Owner+Summary&cells=tiles
[Manual Bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3AStability-LibFuzzer+-label%3AClusterFuzz&sort=-modified&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids
[OSS Trophies]: http://llvm.org/docs/LibFuzzer.html#trophies
[Guided in-process fuzzing of Chrome components]: https://security.googleblog.com/2016/08/guided-in-process-fuzzing-of-chrome.html
[libFuzzer Infrastructure Bugs]: https://bugs.chromium.org/p/chromium/issues/list?q=label:LibFuzzer-Infra
