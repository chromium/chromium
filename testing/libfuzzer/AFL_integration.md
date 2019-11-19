# AFL Integration

This document describes AFL's integration with Chromium. This document is only
for the curious, developers writing Chromium fuzz targets shouldn't worry about
AFL, as this document will explain. Therefore, it does not explain how you
should use AFL locally, in most cases you should just use libFuzzer.

## What?

Nearly every libFuzzer target that runs on ClusterFuzz is also fuzzed on
ClusterFuzz using [AFL]. AFL pioneered the technique of coverage-guided fuzzing
and is similar to libFuzzer. In ClusterFuzz we primarily use libFuzzer, though
we find using AFL also helps. If you are writing a libFuzzer target (unless it
uses [LPM], which AFL does not support) you don't need to do anything to get
fuzzing with AFL on ClusterFuzz.

## Why?

Why use AFL if we already use libFuzzer? The answer is because using AFL helps
us find bugs that we may not find with libFuzzer. We think this is particularly
true for fuzz targets that are slow, memory-intensive, or frequently crash. That
is because AFL's architecture allows it to continue fuzzing even when a crash or
timeout has occurred.

## How?

We use Clang's [trace-pc-guard] and [ASan] to instrument fuzz targets. We use
[afl_driver.cpp] to send coverage information to `afl-fuzz` from the target and
send inputs from `afl-fuzz` to the target. It uses both deferred forkserver mode
and persistent mode. On ClusterFuzz we have a [launcher] to run `afl-fuzz` on
fuzz targets, just like we have for libFuzzer. The launcher also reports and
reproduces crashes, and saves the corpus found during fuzzing. Another function
of the launcher is ensuring targets can be fuzzed well with AFL even if they
would otherwise have an issue with AFL.

We only use AFL to fuzz ASan-instrumented release builds on ClusterFuzz, instead
of using it to fuzz the many different build configurations we use libFuzzer on
(e.g. MSan, UBSan, etc). That is because ASan builds tend to find the most
important bugs and doing a new build for each of the configurations would be too
complicated.

## Trophies

* [AFL Chromium bugs] - bugs found by AFL in Chromium.
* [AFL OSS-Fuzz bugs] - bugs found by AFL in [OSS-Fuzz].

[AFL]: http://lcamtuf.coredump.cx/afl/
[AFL Chromium bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=afl_chrome_asan+-status%3AWontFix%2CDuplicate+label%3Aclusterfuzz
[AFL OSS-Fuzz bugs]: https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=label%3AEngine-afl%2CStability-AFL+label%3AClusterFuzz+-status%3AWontFix%2CDuplicate
[trace-pc-guard]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/afl/src/llvm_mode/README.llvm#169
[ASan]: https://clang.llvm.org/docs/AddressSanitizer.html
[afl_driver.cpp]: https://chromium.googlesource.com/chromium/llvm-project/compiler-rt/lib/fuzzer.git/+/HEAD/afl/afl_driver.cpp
[launcher]: https://github.com/google/clusterfuzz/blob/master/src/python/bot/fuzzers/afl/launcher.py
[LPM]: libprotobuf-mutator.md
[OSS-Fuzz]: https://github.com/google/oss-fuzz/
